#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/debugobjects.h>
#include <linux/kallsyms.h>
#include <linux/list.h>
#include <linux/notifier.h>
#include <linux/rbtree.h>
#include <linux/radix-tree.h>
#include <linux/rcupdate.h>
#include <linux/pfn.h>
#include <linux/kmemleak.h>
#include <linux/atomic.h>
#include <linux/compiler.h>
#include <linux/llist.h>
#include <linux/bitops.h>
#include <asm/pgalloc.h>
#include <linux/uaccess.h>
#include <asm/tlbflush.h>
#include <asm/shmparam.h>
#include <linux/swap.h>
#include "../nova.h"
#include "daxvm.h"

atomic64_t nova_curr_pages = ATOMIC64_INIT(0);
atomic64_t nova_pmem_pages = ATOMIC64_INIT(0);

void nova_clear_persistent_pte(struct super_block*sb, struct nova_inode_info_header *sih, pte_t *ptep, unsigned long addr, unsigned long end)
{
  pte_t *pte;
  unsigned long start;
  unsigned long nr_to_flush = 0;
	unsigned long start_to_flush =0;

  WARN_ON(!ptep);

  if(!end) return;
  start = addr;
  pte = ptep;

	if(!(start & (~PMD_MASK))){
				nova_free_data_blocks(sb, sih, nova_get_addr_off(NOVA_SB(sb),ptep)>>PAGE_SHIFT, 1);
		    atomic64_fetch_dec(&nova_pmem_pages);
		    return;
  }

	nr_to_flush = ((unsigned long)(pte) & (CACHELINE_SIZE - 1))/sizeof(pte_t);
	start_to_flush = (unsigned long)pte;
  do {
		WARN_ON(!pte);
    if (pte_none(*pte))
    	continue;

    set_pte(pte, __pte(0));
		nr_to_flush-=1;
		if(nr_to_flush==0){
    	nova_flush_buffer((void*)start_to_flush, CACHELINE_SIZE, 0);
			start_to_flush = (unsigned long)(pte + 1);
			nr_to_flush = CACHELINE_SIZE/sizeof(pte_t);
		}
  } while (pte++, addr += PAGE_SIZE, addr != end);
		
	if(nr_to_flush) nova_flush_buffer((void*)start_to_flush, CACHELINE_SIZE, 0);
}


void nova_clear_persistent_pmd(struct super_block*sb, struct nova_inode_info_header *sih,pmd_t *pmdp, unsigned long addr, unsigned long end)
{
    pmd_t *pmd;
    pte_t *pte;
    unsigned long next;
    unsigned long start;

    pmd = pmdp;
    start = addr;
    do {
    	next = pmd_addr_end(addr, end);
      	if (pmd_none(*pmd)){
        	continue;
        }
      	pte = pte_offset_kernel(pmd,addr);
      	nova_clear_persistent_pte( sb, sih, pte, addr, next);
				if(!(addr & (~PMD_MASK)) && (start & (~PUD_MASK))){
      		set_pmd(pmd, __pmd(0));
        	nova_flush_buffer(pmd, sizeof(pmd_t), 0);
				}
    } while (pmd++, addr = next, addr != end);
 
    if(!(start & (~PUD_MASK))){
				nova_free_data_blocks(sb, sih, nova_get_addr_off(NOVA_SB(sb),pmdp)>>PAGE_SHIFT, 1);
	    	atomic64_fetch_dec(&nova_pmem_pages);
    }
}


void nova_clear_persistent_pud(struct super_block*sb, struct nova_inode_info_header *sih,pud_t *pudp, unsigned long addr, unsigned long end)
{
    pud_t *pud;
    pmd_t *pmd;
    unsigned long next;
    unsigned long start;

    start = addr;
    pud = pudp;
    do {
      	next = pud_addr_end(addr, end);
      	if (pud_none(*pud))
        	continue;      
      	pmd = pmd_offset(pud,addr);
      	nova_clear_persistent_pmd(sb, sih, pmd, addr, next);
	      if(!(addr & (~PUD_MASK)) && (start & (~PGDIR_MASK))){
      		set_pud(pud, __pud(0));
        	nova_flush_buffer(pud, sizeof(pud_t), 0);
        }         
    } while (pud++, addr = next, addr != end);

    if(!(start & (~PGDIR_MASK))){
			nova_free_data_blocks(sb, sih, nova_get_addr_off(NOVA_SB(sb),pudp)>>PAGE_SHIFT, 1);
			atomic64_fetch_dec(&nova_pmem_pages);
    }
}


void nova_clear_persistent_page_tables(struct super_block*sb, struct nova_inode_info_header *sih,pgd_t * pgdp, unsigned long pgoff, unsigned long end) {

  	pgd_t * pgd;
  	pud_t * pud;
  	unsigned long next;
  	unsigned long start;
	
		start = pgoff;
  	pgd = pgd_offset_pgd(pgdp, pgoff);
  	do {
    		next = pgd_addr_end(pgoff, end);
    		if (pgd_none(*pgd))
     			 continue;

    		pud = (pud_t*) pgd; //4 level paging
    		nova_clear_persistent_pud(sb, sih, pud, pgoff, next);
		    if(!(pgoff & (~PGDIR_MASK))){
      			set_pgd(pgd, __pgd(0));
      			nova_flush_buffer(pgd, sizeof(pgd_t), 0);
    		}
  	} while (pgd++, pgoff = next, pgoff != end);

  	if(start == 0){
				nova_free_data_blocks(sb, sih, nova_get_addr_off(NOVA_SB(sb),pgdp)>>PAGE_SHIFT, 1);
	  		atomic64_fetch_dec(&nova_pmem_pages);
  	}
}

//Mapping bottom-up
static int nova_set_persistent_pte( struct inode *inode, struct super_block*sb, struct nova_inode_info_header *sih,pte_t *ptep, unsigned long addr, unsigned long end, pgprot_t prot, pfn_t pfn_begin, int *nr)
{
	pte_t entry;
	pfn_t pfn;
  pte_t *pte = ptep;

	unsigned long nr_to_flush = 0;
	unsigned long start_to_flush =0;
	
	nr_to_flush = ((unsigned long)(pte) & (CACHELINE_SIZE - 1))/sizeof(pte_t);
	start_to_flush = (unsigned long)pte;

	do {
    		pfn = pfn_to_pfn_t(pfn_t_to_pfn(pfn_begin)+(*nr));
    		if(!pfn_t_valid(pfn)){
					pr_crit("0x%llx\n", pfn_t_to_pfn(pfn));
					BUG();
				}
		
    		if(pfn_t_devmap(pfn))
					entry=pte_mkdevmap(pfn_t_pte(pfn, prot));
				else
	    		entry=pte_mkspecial(pfn_t_pte(pfn, prot));
		
				entry=pte_mkwrite(entry);
        entry=pte_mkyoung(entry);
        entry=pte_mkdirty(entry);
				set_pte(pte, entry);
				nr_to_flush-=1;
				if(nr_to_flush==0){
  					nova_flush_buffer((void*)start_to_flush, CACHELINE_SIZE, 0);
						start_to_flush = (unsigned long)(pte + 1);
						nr_to_flush = CACHELINE_SIZE/sizeof(pte_t);
				}

				(*nr)++;
	} while (pte++, addr += PAGE_SIZE, addr != end);

	if(nr_to_flush) nova_flush_buffer((void*)start_to_flush, CACHELINE_SIZE, 0);
	return 0;
}

static int nova_migrate_persistent_pmd(  struct inode *inode,struct nova_inode_info_header *sih, pmd_t *pmdp_dram, pmd_t *pmdp_pmem, unsigned long addr, unsigned long end, int *nr)
{
  	pte_t *pte_pmem;
  	pte_t *pte_dram;
		unsigned long next;
  	pmd_t *pmd_dram = pmdp_dram;
  	pmd_t *pmd_pmem = pmdp_pmem;

		do {
	  	next = pmd_addr_end(addr, end);
    	pte_dram = pte_offset_kernel(pmd_dram, addr);
	  	if (!pte_dram)
		  	return -ENOMEM;

			pte_pmem = persistent_pte_alloc_nova(  inode, pmd_pmem, addr);
	  	if (!pte_pmem)
		  	return -ENOMEM;

    	if(pmd_devmap(*pmd_dram))
				set_pmd(pmd_pmem,pmd_mkdevmap(*pmd_pmem));

    		__copy_from_user_inatomic_nocache(pte_pmem,pte_dram,PAGE_SIZE);
			pmd_dram++;
	} while (pmd_pmem++, addr = next, addr != end);
	return 0;
}

static int nova_set_persistent_pmd( struct inode *inode, struct super_block*sb, struct nova_inode_info_header *sih,pmd_t *pmdp, unsigned long addr, unsigned long end, pgprot_t prot, pfn_t pfn, int *nr)
{
	pte_t *pte;
	unsigned long next;
  	pmd_t *pmd = pmdp;

	do {
			next = pmd_addr_end(addr, end);
			pte = persistent_pte_alloc_nova( inode, pmd, addr);
	  	if (!pte)
		  	return -ENOMEM;

			if (!(addr & (PMD_SIZE-1)) && !(next & (PMD_SIZE-1)) && !((pfn_t_to_pfn(pfn)+(*nr)) & 511)){
				set_pmd(pmd,pmd_mkdevmap(*pmd));
			}

		if (nova_set_persistent_pte( inode, sb, sih, pte, addr, next, prot, pfn, nr))
			return -ENOMEM;
				
	} while (pmd++, addr = next, addr != end);
	return 0;
}

static int nova_migrate_persistent_pud(  struct inode *inode,  struct nova_inode_info_header *sih, pud_t *pudp_dram, pud_t *pudp_pmem, unsigned long addr, unsigned long end, int *nr)
{
		pmd_t *pmd_dram;
		pmd_t *pmd_pmem;
		unsigned long next;
  	pud_t *pud_dram = pudp_dram;
  	pud_t *pud_pmem = pudp_pmem;

		do {
	 		next = pud_addr_end(addr, end);
      pmd_dram = pmd_offset(pud_dram, addr);
	  	if (!pmd_dram)
				return -ENOMEM;

    		pmd_pmem = persistent_pmd_alloc_nova(  inode, pud_pmem, addr);
	  	if (!pmd_pmem)
				return -ENOMEM;

			if (nova_migrate_persistent_pmd(  inode,  sih, pmd_dram, pmd_pmem, addr, next, nr))
				return -ENOMEM;

      pud_dram++;
		} while (pud_pmem++, addr = next, addr != end);
		return 0;
}

static int nova_set_persistent_pud( struct inode *inode, struct super_block*sb, struct nova_inode_info_header *sih,pud_t *pudp, unsigned long addr,
		unsigned long end, pgprot_t prot, pfn_t pfn, int *nr)
{
	pmd_t *pmd;
	unsigned long next;
  pud_t *pud = pudp;

	do {
	 	next = pud_addr_end(addr, end);
    pmd = persistent_pmd_alloc_nova( inode, pud, addr);

	  if (!pmd)
		 	return -ENOMEM;

		if (nova_set_persistent_pmd( inode, sb, sih, pmd, addr, next, prot, pfn, nr))
			return -ENOMEM;
	} while (pud++, addr = next, addr != end);
	return 0;
}

static int nova_migrate_persistent_p4d(  struct inode *inode,   struct nova_inode_info_header *sih, p4d_t *p4dp_dram, p4d_t *p4dp_pmem, unsigned long addr,	unsigned long end, int *nr)
{
	pud_t *pud_dram;
	pud_t *pud_pmem;
	unsigned long next;
  p4d_t *p4d_dram = p4dp_dram;
  p4d_t *p4d_pmem = p4dp_pmem;

	do {
			next = p4d_addr_end(addr, end);
      pud_dram = pud_offset(p4d_dram, addr);
	  	if (!pud_dram)
				return -ENOMEM;
    	pud_pmem = persistent_pud_alloc_nova(  inode, p4d_pmem, addr);
	  	if (!pud_pmem)
				return -ENOMEM;

			if (nova_migrate_persistent_pud(  inode,  sih, pud_dram, pud_pmem, addr, next, nr))
				return -ENOMEM;
    
      p4d_dram++;
	} while (p4d_pmem++, addr = next, addr != end);
	return 0;
}
static int nova_set_persistent_p4d( struct inode *inode,  struct super_block*sb, struct nova_inode_info_header *sih, p4d_t *p4dp, unsigned long addr,
														unsigned long end, pgprot_t prot, pfn_t pfn, int *nr)
{
	pud_t *pud;
	unsigned long next;
  p4d_t *p4d = p4dp;

	do {
			next = p4d_addr_end(addr, end);
    	pud = persistent_pud_alloc_nova( inode, p4d, addr);
	  	if (!pud)
				return -ENOMEM;

			if (nova_set_persistent_pud( inode, sb, sih, pud, addr, next, prot, pfn, nr))
				return -ENOMEM;

	} while (p4d++, addr = next, addr != end);
	return 0;
}

static int nova_migrate_persistent_pgd(  struct inode *inode,  struct nova_inode_info_header *sih, pgd_t *pgd1_dram, pgd_t *pgd1_pmem, unsigned long start, unsigned long end, int *nr)
{
	pgd_t *pgd_dram, *pgd_pmem;
	p4d_t *p4d_dram, *p4d_pmem;
	unsigned long next;
	unsigned long addr = start;
	int err = 0;

	BUG_ON(addr >= end);
	pgd_dram = pgd_offset_pgd(pgd1_dram, addr);
	pgd_pmem = pgd_offset_pgd(pgd1_pmem, addr);
	do {
		next = pgd_addr_end(addr, end);
    p4d_dram = p4d_offset(pgd_dram, addr);
	 	if (!p4d_dram)
			  return -ENOMEM;
	  p4d_pmem = persistent_p4d_alloc_nova(  inode, pgd_pmem, addr);
	  if (!p4d_pmem)
			  return -ENOMEM;
		err = nova_migrate_persistent_p4d(  inode,  sih, p4d_dram, p4d_pmem, addr, next, nr);
		if (err)
			return err;

    pgd_dram++;
	} while (pgd_pmem++, addr = next, addr != end);
	return 0;
}

static int nova_set_persistent_pgd( struct inode *inode, struct super_block*sb, struct nova_inode_info_header *sih,pgd_t *pgd1, unsigned long start, unsigned long end, pgprot_t prot, pfn_t pfn, int *nr)
{
	pgd_t *pgd;
	p4d_t *p4d;
	unsigned long next;
	unsigned long addr = start;
	int err = 0;

	BUG_ON(addr >= end);
	pgd = pgd_offset_pgd(pgd1, addr);
	do {
		next = pgd_addr_end(addr, end);
	  	p4d = persistent_p4d_alloc_nova( inode, pgd, addr);
	  	if (!p4d)
			return -ENOMEM;

		err = nova_set_persistent_p4d( inode, sb, sih, p4d, addr, next, prot, pfn, nr);
		if (err)
			return err;
	} while (pgd++, addr = next, addr != end);
	return 0;
}

int nova_migrate_dram_to_pmem_tables( struct inode *inode, struct super_block*sb, struct nova_inode_info_header *sih, unsigned long count)
{
	int ret;
	unsigned long start;
	unsigned long size;
  int nr = 0;
	struct nova_inode *pi;

  pgd_t *pgd=NULL;
	pud_t *pud=NULL;
	pmd_t *pmd=NULL;
  pte_t *pte=NULL;
	
	start = 0;
  size = (unsigned long)count << PAGE_SHIFT;
  ret=-1;

  	//hugepage
  	while ((start+size) > sih->ppt_ceiling || ((((start+size)==PMD_SIZE)) && (sih->ppt_level > PMD_LEVEL))) {
    		sih->ppt_level--;
  
    		if(sih->ppt_level == PTE_LEVEL){
      			pte = (pte_t*) __r_p_pte_alloc_nova(  inode, NULL, 0);
      			WARN_ON(!pte);
      			sih->ppgd=pte;
      			sih->ppt_ceiling=PMD_SIZE;

						pi = nova_get_inode(sb, inode);
						pi->ppgd=nova_get_addr_off(NOVA_SB(sb), pte);
      			nova_flush_buffer(&pi->ppgd, CACHELINE_SIZE, 0);

    		}	

    		if(sih->ppt_level == PMD_LEVEL){ 
      			pmd = (pmd_t*) __r_p_pmd_alloc_nova(  inode, NULL,0);
      			WARN_ON(!pmd);
      			if(sih->ppgd) persistent_pmd_populate_nova(pmd,(pte_t*)sih->vpgd);
      			sih->ppgd=pmd;
      			sih->ppt_ceiling=PUD_SIZE;

						pi = nova_get_inode(sb, inode);
						pi->ppgd=nova_get_addr_off(NOVA_SB(sb), pmd);
      			nova_flush_buffer(&pi->ppgd, CACHELINE_SIZE, 0);
    		}

    		if(sih->ppt_level == PUD_LEVEL){ 
      			pud = (pud_t*) __r_p_pud_alloc_nova(  inode, NULL, 0);
      			WARN_ON(!pud);
      			if(sih->ppgd)  persistent_pud_populate_nova(pud,(pmd_t *)sih->vpgd);
      			sih->ppgd=pud;
      			sih->ppt_ceiling=PGDIR_SIZE;

						pi = nova_get_inode(sb, inode);
						pi->ppgd=nova_get_addr_off(NOVA_SB(sb), pud);
      			nova_flush_buffer(&pi->ppgd, CACHELINE_SIZE, 0);
    		}

    		if(sih->ppt_level == PGD_LEVEL){
      			pgd = (pgd_t *)nova_pt_alloc( inode);
      			WARN_ON(!pgd);
      			if(sih->ppgd)  persistent_pgd_populate_nova((pgd_t*) pgd,(p4d_t *)sih->vpgd);
      			sih->ppgd=pgd;
      			sih->ppt_ceiling=0xffffffff;

						pi = nova_get_inode(sb, inode);
						pi->ppgd=nova_get_addr_off(NOVA_SB(sb), pgd);
      			nova_flush_buffer(&pi->ppgd, CACHELINE_SIZE, 0);
    		}	 
  	}

  	//WARN_ON(sih->ppgd==NULL);

  	if (sih->ppt_level==PTE_LEVEL) {
      		  __copy_from_user_inatomic_nocache(((pte_t*)sih->ppgd)+pte_index(start), ((pte_t*)sih->vpgd)+pte_index(start), PAGE_SIZE);
  	}
  	else if (sih->ppt_level==PMD_LEVEL) {
	  	 ret = nova_migrate_persistent_pmd(  inode, sih, ((pmd_t*)sih->vpgd)+pmd_index(start), ((pmd_t*)sih->ppgd)+pmd_index(start), start, start+size, &nr);
  	}
  	else if(sih->ppt_level==PUD_LEVEL) {
		 ret = nova_migrate_persistent_pud(  inode, sih, ((pud_t*)sih->vpgd)+pud_index(start), ((pud_t*)sih->ppgd)+pud_index(start), start, start+size, &nr);
  	}
  	else if(sih->ppt_level==PGD_LEVEL) {
		 ret = nova_migrate_persistent_pgd(  inode, sih, sih->vpgd, sih->ppgd, start, start+size, &nr);
  	}
  	return ret;
}
int nova_update_persistent_page_table( struct inode *inode, struct super_block*sb, struct nova_inode_info_header *sih, unsigned long count, pgprot_t prot, pfn_t pfn, unsigned long pgoff)
{
	int ret;
	unsigned long start;
	unsigned long size;
  int nr = 0;
	struct nova_inode *pi;

  pgd_t *pgd=NULL;
	pud_t *pud=NULL;
	pmd_t *pmd=NULL;
  pte_t *pte=NULL;
	
	start = pgoff << PAGE_SHIFT;
  size = (unsigned long)count << PAGE_SHIFT;
  ret=-1;

  	while ((start+size) > sih->ppt_ceiling || ((((start+size)==PMD_SIZE)) && (sih->ppt_level > PMD_LEVEL))) {
    		sih->ppt_level--;
  
    		if(sih->ppt_level == PTE_LEVEL){
      			pte = (pte_t*) __r_p_pte_alloc_nova( inode, NULL, 0);
      			WARN_ON(!pte);
      			sih->ppgd=pte;
      			sih->ppt_ceiling=PMD_SIZE;

						pi = nova_get_inode(sb, inode);
						pi->ppgd=nova_get_addr_off(NOVA_SB(sb), pte);
      			nova_flush_buffer(&pi->ppgd, CACHELINE_SIZE, 0);
    		}	

    		if(sih->ppt_level == PMD_LEVEL){ 
      			pmd = (pmd_t*) __r_p_pmd_alloc_nova( inode, NULL,0);
      			WARN_ON(!pmd);
      			if(sih->ppgd) persistent_pmd_populate_nova(pmd,(pte_t*)sih->ppgd);
      			sih->ppgd=pmd;
      			sih->ppt_ceiling=PUD_SIZE;
      			
						pi = nova_get_inode(sb, inode);
						pi->ppgd=nova_get_addr_off(NOVA_SB(sb), pmd);
      			nova_flush_buffer(&pi->ppgd, CACHELINE_SIZE, 0);
    		}

    		if(sih->ppt_level == PUD_LEVEL){ 
      			pud = (pud_t*) __r_p_pud_alloc_nova( inode, NULL, 0);
      			WARN_ON(!pud);
      			if(sih->ppgd)  persistent_pud_populate_nova(pud,(pmd_t *)sih->ppgd);
      			sih->ppgd=pud;
      			sih->ppt_ceiling=PGDIR_SIZE;
						
						pi = nova_get_inode(sb, inode);
						pi->ppgd=nova_get_addr_off(NOVA_SB(sb), pud);
      			nova_flush_buffer(&pi->ppgd, CACHELINE_SIZE, 0);
    		}

    		if(sih->ppt_level == PGD_LEVEL){
      			pgd = (pgd_t *)nova_pt_alloc( inode);
      			WARN_ON(!pgd);
      			if(sih->ppgd)  persistent_pgd_populate_nova((pgd_t*) pgd,(p4d_t *)sih->ppgd);
      			sih->ppgd=pgd;
      			sih->ppt_ceiling=0xffffffff;
		
						pi = nova_get_inode(sb, inode);
						pi->ppgd=nova_get_addr_off(NOVA_SB(sb), pgd);
      			nova_flush_buffer(&pi->ppgd, CACHELINE_SIZE, 0);
    		}	 
  	}

  	WARN_ON(sih->ppgd==NULL);

  	if (sih->ppt_level==PTE_LEVEL) {
		ret = nova_set_persistent_pte( inode, sb,sih,((pte_t*)sih->ppgd)+pte_index(start), start, start+size, prot, pfn, &nr);
  	}
  	else if (sih->ppt_level==PMD_LEVEL) {
	  	ret = nova_set_persistent_pmd( inode, sb,sih,((pmd_t*)sih->ppgd)+pmd_index(start), start, start+size, prot, pfn, &nr);
  	}
  	else if(sih->ppt_level==PUD_LEVEL) {
		ret = nova_set_persistent_pud( inode, sb,sih,((pud_t*)sih->ppgd)+pud_index(start), start, start+size, prot, pfn, &nr);
  	}
  	else if(sih->ppt_level==PGD_LEVEL) {
		ret = nova_set_persistent_pgd( inode, sb,sih,sih->ppgd, start, start+size, prot, pfn, &nr);
  	}
  	return ret;
}

pmd_t nova_get_persistent_pmd(struct nova_inode_info_header *sih, unsigned long addr, bool *huge)
{

	pgd_t *pgd=NULL;
	p4d_t *p4d=NULL;
	pud_t *pud=NULL;
	pmd_t *pmd=NULL;
    	pmd_t ret;

  if (sih->ppt_level==PGD_LEVEL){
    		pgd = pgd_offset_pgd((pgd_t *)sih->ppgd,addr);
	  	  if(pgd && !pgd_none(*pgd))
	    		p4d = p4d_offset(pgd, addr);
		    if(p4d && !p4d_none(*p4d))
	    	  pud = pud_offset(p4d, addr);
		    if(pud && !pud_none(*pud))
	    		pmd = pmd_offset(pud, addr);
		    ret = *pmd;
  }    
	else if (sih->ppt_level==PUD_LEVEL){
		  pud = ((pud_t*)sih->ppgd) + pud_index(addr);
	  	if(pud && !pud_none(*pud)) 
      			pmd = pmd_offset(pud, addr);
	  	ret = *pmd;
  	}
	else if (sih->ppt_level==PMD_LEVEL){
    		pmd = ((pmd_t*)sih->ppgd) + pmd_index(addr);
	  	  ret = *pmd;
  }
	else if (sih->ppt_level==PTE_LEVEL)
    		pmd_populate_kernel(NULL, &ret, (pte_t*) sih->ppgd);
	else BUG();

	if(*huge) {	
		if (pmd_devmap(ret)) *huge=1;
		else *huge=0;
	}
  return ret;
}


void *nova_pt_alloc( struct inode *inode){
		unsigned long newblock, allocated;
  	int err;
		struct super_block *sb = inode->i_sb;
		struct nova_inode_info_header *sih = NOVA_IH(inode);
		allocated = nova_new_data_blocks(sb, sih, &newblock, 0, 1, 1, ANY_CPU, 1);
		WARN_ON(!newblock);
		atomic64_fetch_inc(&nova_pmem_pages);
  	return nova_get_block(sb, nova_get_block_off(sb, newblock, 0)); 
}


int __p_pte_alloc_nova( struct inode *inode, pmd_t *pmd, unsigned long address)
{
        pte_t *new = (pte_t*)nova_pt_alloc( inode);
        if (!new)
          return -ENOMEM;
        smp_wmb();
        if(pmd)
          	persistent_pmd_populate_nova(pmd, new); 
        return 0;
}

pte_t * __r_p_pte_alloc_nova( struct inode *inode, pmd_t *pmd, unsigned long address)
{
        pte_t *new = (pte_t*)nova_pt_alloc( inode);
        if (!new)
          return	NULL;
        smp_wmb(); 
        if(pmd)
          persistent_pmd_populate_nova(pmd, new);
        return new;
}


int __p_pud_alloc_nova( struct inode *inode, p4d_t *p4d, unsigned long address)
{
        pud_t *new = (pud_t*) nova_pt_alloc( inode);
        if (!new)
          return -ENOMEM;
        smp_wmb();
        if(p4d)
          persistent_p4d_populate_nova(p4d, new);
        return 0;
}


pud_t * __r_p_pud_alloc_nova( struct inode *inode, p4d_t *p4d, unsigned long address)
{
        pud_t *new = (pud_t*) nova_pt_alloc( inode);
        if (!new)
          return NULL;
        smp_wmb(); 
        if(p4d)
          persistent_p4d_populate_nova(p4d, new);
        return new;
}

int __p_pmd_alloc_nova( struct inode *inode, pud_t *pud, unsigned long address)
{
        pmd_t *new = (pmd_t*)nova_pt_alloc( inode);
        if (!new)
          return -ENOMEM;
        smp_wmb(); 
        if(pud)
          persistent_pud_populate_nova(pud, new);
        return 0;
}

pmd_t* __r_p_pmd_alloc_nova( struct inode *inode, pud_t *pud, unsigned long address)
{
        pmd_t *new = (pmd_t*)nova_pt_alloc( inode);
        if (!new)
          return NULL;
        smp_wmb(); 
        if(pud)
          persistent_pud_populate_nova(pud, new);
        return new;
}
