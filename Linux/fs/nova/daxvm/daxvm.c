#include <linux/module.h>
#include <linux/buffer_head.h>
#include <linux/cpufeature.h>
#include <asm/pgtable.h>
#include <linux/version.h>
#include "../nova.h"
#include <linux/kernel.h>
#include <linux/syscalls.h>

#include "daxvm.h"
#include "../../daxvm.h"

unsigned long nova_daxvm_pmem_threshold = 32*1024;
int nova_persistent_page_tables=0;

bool nova_daxvm_validate_tables(struct inode *inode){
  struct nova_inode_info_header *sih = NOVA_IH(inode);
	unsigned long num_pages = inode->i_size/PAGE_SIZE + ((inode->i_size%PAGE_SIZE)!=0);
	unsigned long pos = 0;
	bool huge_page;
	pmd_t nova_pmd_pmem;
	pmd_t nova_pmd_dram;
	uint64_t pfn_pmem;
	uint64_t pfn_dram;
	int ret = 0;
	int i;
	for (i=0; i<num_pages; i++){
		nova_pmd_pmem = nova_get_persistent_pmd(sih, round_down(pos, PMD_SIZE), &huge_page);
		nova_pmd_dram = nova_get_volatile_pmd(sih, round_down(pos, PMD_SIZE), &huge_page);
		
		if(pmd_none(nova_pmd_pmem) || !pte_present(*pte_offset_kernel(&nova_pmd_pmem,pos))){
						if(pmd_none(nova_pmd_dram) || !pte_present(*pte_offset_kernel(&nova_pmd_dram,pos)))
										return ret;
    				else {
										ret=-1;
										return ret;
						}
		}
		pfn_pmem = pte_pfn(*pte_offset_kernel(&nova_pmd_pmem,pos));
		pfn_dram = pte_pfn(*pte_offset_kernel(&nova_pmd_dram,pos));
		if(pfn_pmem != pfn_dram){
						ret=-1;
						return ret;
		}
		pos+=PAGE_SIZE;
	}
	return 0;
}

pmd_t * nova_daxvm_page_walk(struct vm_fault *vmf){
	
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;

	pgd = pgd_offset(vmf->vma->vm_mm, vmf->address);
	if(!pgd) goto out;
	
  p4d = p4d_offset(pgd, vmf->address);
	if (!p4d) goto out;

	pud = pud_offset(p4d, vmf->address);
	if (!pud) goto out;

	pmd = pmd_offset(pud, vmf->address);
	return pmd;
out:
	return NULL;
}

bool nova_daxvm_file_mkwrite(struct vm_fault *vmf){
	
	struct vm_area_struct *vma = vmf->vma;	
	unsigned long address = vma->vm_start;
	unsigned long end = vma->vm_end;
	struct mm_struct *mm = vma->vm_mm;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	bool ret=0;

	while (address < end){
    		pgd = pgd_offset(mm, address);
		if(!pgd) break;
			
    		p4d = p4d_alloc(mm, pgd, address);
		if (!p4d) break;

		pud = pud_alloc(mm, p4d, address);
		if (!pud) break;

		pmd = pmd_alloc(mm, pud, address);
		if (!pmd) break;
	
		*pmd=pmd_mkwrite(pmd_mkdirty(*pmd));
		
		if((vmf->address&PMD_MASK)==address){
			if(is_pmd_daxvm(*pmd))
				ret = pte_present(*pte_offset_kernel(pmd,vmf->address));
			else
				ret=1;
		}

		address+=PMD_SIZE; 
	}
out: 
	return ret;
}


bool nova_daxvm_set_pmd(struct vm_area_struct *vma, pmd_t *pmd, unsigned long address, unsigned long pgoff, 
					bool persistent, bool wrprotect, struct nova_inode_info_header *sih, loff_t size){

	pmd_t nova_pmd;
	spinlock_t *ptl;
	bool ret = 0;
	struct mm_struct *mm=vma->vm_mm;
	bool huge_page = 0;	
	unsigned long pfn;

	if(size==PMD_SIZE)
		huge_page=1;

	if(!pmd) goto out;

	if(!pmd_none(*pmd)){
		if(!is_pmd_daxvm(*pmd)){
			goto out;
		}
		ptl = pmd_lock(mm, pmd);
		goto already_set;
	}
retry:

	if(sih->ppgd)
			nova_pmd = nova_get_persistent_pmd(sih, (round_down(pgoff, PMD_SIZE>>PAGE_SHIFT))<<PAGE_SHIFT, &huge_page);
	else if (sih->vpgd)
			nova_pmd = nova_get_volatile_pmd(sih, (round_down(pgoff, PMD_SIZE>>PAGE_SHIFT))<<PAGE_SHIFT, &huge_page);
	/*
  	if (sih->vpgd && (persistent==0)) {
			nova_pmd = nova_get_volatile_pmd(sih, (round_down(pgoff, PMD_SIZE>>PAGE_SHIFT))<<PAGE_SHIFT, &huge_page);
  	}
  	else if(sih->ppgd){
			nova_pmd = nova_get_persistent_pmd(sih, (round_down(pgoff, PMD_SIZE>>PAGE_SHIFT))<<PAGE_SHIFT, &huge_page);
  	}
*/
		//else {
		    //nova_daxvm_build_tables_from_tree(vma->vm_file->f_mapping->host);
		    //goto retry;
		//}
  
	if(pmd_none(nova_pmd)){
		goto out;
	}
		
	ptl = pmd_lock(mm, pmd);

	//huge page promotion
	if(huge_page && __transparent_hugepage_enabled(vma)){ 
		pfn = pte_pfn(*pte_offset_kernel(&nova_pmd,round_down(address, PMD_SIZE)));
		nova_pmd = pmd_mkhuge(pfn_pmd(pfn, vma->vm_page_prot));
		nova_pmd = pmd_mkdevmap(nova_pmd);
	  if(vma->vm_flags & VM_DAXVM_EPHEMERAL)
    		nova_pmd=pmd_mk_daxvm_ephemeral(nova_pmd);
		goto set;
	}
	nova_pmd = pmd_mk_daxvm(nova_pmd);
	if(vma->vm_flags & VM_DAXVM_EPHEMERAL)
    		nova_pmd=pmd_mk_daxvm_ephemeral(nova_pmd);
set:
	set_pmd_at(mm, round_down(address,PMD_SIZE), pmd, nova_pmd);

already_set:
	if(is_pmd_daxvm(*pmd)){
		if (wrprotect) {
			*pmd=pmd_wrprotect(*pmd);
		}
		else *pmd=pmd_mkwrite(pmd_mkdirty(*pmd));
    		BUG_ON(!pmd);
    		BUG_ON(pmd_none(*pmd));
    		if(pte_offset_kernel(pmd,address))
		  		ret = pte_present(*pte_offset_kernel(pmd,address));
    		else
      		ret = 0;
				//if(!ret) pr_crit("Not found %d %d 0x%llx\n", sih->vfs_inode.i_ino, pgoff, address);
	}
	else if (pmd_devmap(*pmd)) {
		if (wrprotect) 	
			*pmd=pmd_wrprotect(*pmd);
		else *pmd=pmd_mkwrite(pmd_mkdirty(*pmd));
		ret=1;
	}

	spin_unlock(ptl);
	return ret;
 
	spin_unlock(ptl);
out:
	return 0;
}

int nova_daxvm_get_pfn(struct inode *inode, loff_t pos, size_t size, pfn_t *pfn){
  struct nova_inode_info_header *sih = NOVA_IH(inode);
	pmd_t nova_pmd;
	bool huge_page = 0;

	if (sih->ppgd){
	  if(size == PMD_SIZE) huge_page=1;
		nova_pmd = nova_get_persistent_pmd(sih, round_down(pos, PMD_SIZE), &huge_page);
	}
	else if (sih->vpgd) {
	  if(size == PMD_SIZE) huge_page=1;
		nova_pmd = nova_get_volatile_pmd(sih, round_down(pos, PMD_SIZE), &huge_page);
	}	
 /*	
	if (sih->vpgd) {
	  if(size == PMD_SIZE) huge_page=1;
		nova_pmd = nova_get_volatile_pmd(sih, round_down(pos, PMD_SIZE), &huge_page);
  }
	else if (sih->ppgd){
	  if(size == PMD_SIZE) huge_page=1;
		nova_pmd = nova_get_persistent_pmd(sih, round_down(pos, PMD_SIZE), &huge_page);
	}	
*/
	else {
    		pr_crit("Unfortunately we somehow have missed a file\n");
				return -1;
	}
	if(pmd_none(nova_pmd) || !pte_present(*pte_offset_kernel(&nova_pmd,pos))){
    		return -1;
	}

	if ((size == PMD_SIZE) && !huge_page) {
		return -1;
	}

	*pfn = pfn_to_pfn_t(pte_pfn(*pte_offset_kernel(&nova_pmd,pos)));
	return 0;
}

bool nova_daxvm_fault(struct vm_fault *vmf, struct inode *inode){

	bool wrprotect = 0; 
	int ret = 0;
  	struct nova_inode_info_header *sih = NOVA_IH(inode);
	loff_t size = PMD_SIZE;

	if (!((vmf->vma->vm_flags & (VM_WRITE|VM_SHARED))==(VM_WRITE|VM_SHARED)) || (!(vmf->flags & FAULT_FLAG_WRITE)))
		wrprotect=1;

	ret = nova_daxvm_set_pmd(vmf->vma, nova_daxvm_page_walk(vmf), vmf->address, vmf->pgoff, 0, wrprotect, sih, size);
	return ret;	
}


//from pmem to dram (currently only when high tlb miss overhead is detected)
void nova_daxvm_migrate_pgtables( struct mm_struct *mm, bool persist){
  	struct vm_area_struct *vma;
		struct super_block *sb;
  	struct inode *inode;
  	struct nova_inode_info_header *sih;
  	//from pmem to dram (currently only when high tlb miss overhead is detected)
  	if(!persist){
    		for (vma = mm->mmap; vma; vma = vma->vm_next) {
      			if(vma->vm_flags & VM_DAXVM && (vma->vm_end-vma->vm_start) > nova_daxvm_pmem_threshold){
        			inode = vma->vm_file->f_mapping->host;
							sb = inode->i_sb;
        			sih = NOVA_IH(inode);
	      			if(!sih->vpgd){
        				nova_migrate_pmem_to_dram_tables(inode, sih, (vma->vm_end-vma->vm_start)>>PAGE_SHIFT);
        				down_read(&mm->mmap_sem);
        				zap_vma_ptes(vma,vma->vm_start, vma->vm_end-vma->vm_start);
        				nova_daxvm_attach_tables(vma,inode); 
        				up_read(&mm->mmap_sem);
	      			}
      			}   
    		}
  	}
}

void nova_daxvm_migrate_file_pgtables( struct inode *inode,  bool persist){
  struct nova_inode_info_header *sih;
	struct vm_area_struct *vma;
	struct address_space *mapping;
	struct super_block *sb = inode->i_sb;
  sih = NOVA_IH(inode);
  //from dram to pmem
	//I never figured out what happened with the locks so I let whatever is mapped as is and never delete the stale tables 
	//I can also let them fault and loose 10 per cent perf
	//FIXME
  if(persist) {
		if(!sih->ppgd){
      //down_write(&NOVA_IH(inode)->i_mmap_sem);
 			nova_migrate_dram_to_pmem_tables( inode, sb, sih, inode->i_size>>PAGE_SHIFT);
			/*
			mapping = inode->i_mapping;
      vma_interval_tree_foreach(vma, &mapping->i_mmap, 0, 0) {
				if(vma->vm_flags & VM_DAXVM) {
						down_read(&vma->vm_mm->mmap_sem);
      			zap_vma_ptes(vma,vma->vm_start, vma->vm_end-vma->vm_start);
      			nova_daxvm_attach_persistent_tables(vma,inode); 
						up_read(&vma->vm_mm->mmap_sem);
				}
			}
			nova_daxvm_delete_tables( inode, sb, 0, inode->i_size, 0);
			*/
      //up_write(&NOVA_IH(inode)->i_mmap_sem);
	 }
	}
}

void nova_daxvm_attach_tables(struct vm_area_struct *vma, struct inode *inode){

	unsigned long pgoff = vma->vm_pgoff;
	unsigned long address = vma->vm_start;
	unsigned long end = vma->vm_end;
	struct mm_struct *mm = vma->vm_mm;
  struct nova_inode_info_header *sih = NOVA_IH(inode);
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	bool wrprotect=0; 
	int ret;
	loff_t size = PAGE_SIZE;

	//if (!((vma->vm_flags & (VM_WRITE|VM_SHARED))==(VM_WRITE|VM_SHARED)) || daxvm_msync_support) 
	//if (!((vma->vm_flags & (VM_WRITE|VM_SHARED))==(VM_WRITE|VM_SHARED))) 
	wrprotect=1;
	
  if ((address + PMD_SIZE) > end){
		 BUG();
		 goto out;	
	}

	while (address < end){
    		pgd = pgd_offset(mm, address);
		if(!pgd) break;
			
    		p4d = p4d_alloc(mm, pgd, address);
		if (!p4d) break;

		pud = pud_alloc(mm, p4d, address);
		if (!pud) break;

		pmd = pmd_alloc(mm, pud, address);
		if (!pmd) break;
	
		if ((end-address) >= PMD_SIZE){	
			size=PMD_SIZE;
		}

		ret = nova_daxvm_set_pmd(vma, pmd, address, pgoff, 0, wrprotect, sih, size); 
    if(!ret) {
			break;
		}	
		address+=PMD_SIZE; 
		pgoff+=(PMD_SIZE>>PAGE_SHIFT); 
	}
out: 
	return;
}


void nova_daxvm_build_tables( struct inode *inode, unsigned long num_pages, unsigned long m_pblk, unsigned long m_lblk, bool persist){
      	
	struct super_block *sb = inode->i_sb;
  struct nova_sb_info *sbi = NOVA_SB(sb);
	struct nova_inode_info_header *sih = NOVA_IH(inode);
	int ret=0;	

  if(persist){
    ret = nova_update_persistent_page_table ( inode, sb, sih, num_pages, PAGE_SHARED_EXEC, pfn_to_pfn_t((sbi->phys_addr + (m_pblk<<PAGE_SHIFT))>>PAGE_SHIFT), m_lblk);
		PERSISTENT_BARRIER();
    BUG_ON(!sih->ppgd);
  }
  else
    ret = nova_update_volatile_page_table (inode, sih, num_pages, PAGE_SHARED_EXEC,pfn_to_pfn_t((sbi->phys_addr + (m_pblk<<PAGE_SHIFT))>>PAGE_SHIFT), m_lblk);
}


void nova_daxvm_delete_tables(  struct nova_inode_info_header *sih, struct super_block *sb, unsigned long start, unsigned long end, bool delete){

	if(start==end) return;
	
	if(sih->ppgd && delete){
    		if (sih->ppt_level==PTE_LEVEL) {
          if(end > (start + PMD_SIZE)) end=PMD_SIZE;
	      			nova_clear_persistent_pte(sb, sih, ((pte_t*)sih->ppgd) + pte_index(start), start, end);
   	 	  }
    		else if (sih->ppt_level==PMD_LEVEL) {
          if(end > (start + PUD_SIZE)) end=PUD_SIZE;
	      			nova_clear_persistent_pmd(sb, sih, ((pmd_t*)sih->ppgd) + pmd_index(start), start, end);
    		}
    		else if(sih->ppt_level==PUD_LEVEL) {
          if(end > (start + PGDIR_SIZE)) end=PGDIR_SIZE;
	     			nova_clear_persistent_pud(sb, sih, ((pud_t*)sih->ppgd) + pud_index(start), start, end);
    		}
    		else if(sih->ppt_level==PGD_LEVEL) {
	      		nova_clear_persistent_page_tables(sb, sih, ((pgd_t*)sih->ppgd), start, end);
    		}

    		if(start == 0) {
    			sih->ppgd=NULL;
    			sih->ppt_ceiling=0; 
    			sih->ppt_level=4;
   	 	}		
		  PERSISTENT_BARRIER();
 }
 if(sih->vpgd ){
    		if (sih->vpt_level==PTE_LEVEL) {
          if(end > (start + PMD_SIZE)) end=PMD_SIZE;
	      			nova_clear_volatile_pte(sih, ((pte_t*)sih->vpgd) + pte_index(start), start, end);
   	 	  }
    		else if (sih->vpt_level==PMD_LEVEL) {
          if(end > (start + PUD_SIZE)) end=PUD_SIZE;
	      			nova_clear_volatile_pmd(sih, ((pmd_t*)sih->vpgd) + pmd_index(start), start, end);
    		}
    		else if(sih->vpt_level==PUD_LEVEL) {
          if(end > (start + PGDIR_SIZE)) end=PGDIR_SIZE;
	     			nova_clear_volatile_pud(sih, ((pud_t*)sih->vpgd) + pud_index(start), start, end);
    		}
    		else if(sih->vpt_level==PGD_LEVEL) {
	      		nova_clear_volatile_page_tables(sih, ((pgd_t*)sih->vpgd), start, end);
    		}

    		if(start == 0) {
    			sih->vpgd=NULL;
    			sih->vpt_ceiling=0; 
    			sih->vpt_level=4;
   	 	}		
 }

}

/*
SYSCALL_DEFINE1(nova_daxvm_migrate_to_dram, unsigned long, pid){

	struct task_struct* t;
	t = find_task_by_vpid(pid);
	struct mm_struct *mm = t->mm;
	nova_daxvm_migrate_pgtables(mm, 0);
}
*/
