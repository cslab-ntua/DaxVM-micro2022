#include <linux/module.h>
#include <linux/buffer_head.h>
#include <linux/cpufeature.h>
#include <asm/pgtable.h>
#include <linux/version.h>
#include "../ext4.h"
#include <linux/kernel.h>
#include <linux/syscalls.h>

#include "daxvm.h"
#include "../../daxvm.h"

unsigned long ext4_daxvm_pmem_threshold = 32*1024;
extern void ext4_daxvm_build_tables_from_tree(struct inode *inode);

bool validate_tables(struct inode *inode){
  struct ext4_inode_info *sih = EXT4_I(inode);
	unsigned long num_pages = inode->i_size/PAGE_SIZE + ((inode->i_size%PAGE_SIZE)!=0);
	unsigned long pos = 0;
	bool huge_page;
	pmd_t ext4_pmd_pmem;
	pmd_t ext4_pmd_dram;
	uint64_t pfn_pmem;
	uint64_t pfn_dram;
	int ret = 0;
	int i;
	for (i=0; i<num_pages; i++){
		ext4_pmd_pmem = ext4_get_persistent_pmd(sih, round_down(pos, PMD_SIZE), &huge_page);
		ext4_pmd_dram = ext4_get_volatile_pmd(sih, round_down(pos, PMD_SIZE), &huge_page);
		
		if(pmd_none(ext4_pmd_pmem) || !pte_present(*pte_offset_kernel(&ext4_pmd_pmem,pos))){
						if(pmd_none(ext4_pmd_dram) || !pte_present(*pte_offset_kernel(&ext4_pmd_dram,pos)))
										return ret;
    				else {
										ret=-1;
										return ret;
						}
		}
		pfn_pmem = pte_pfn(*pte_offset_kernel(&ext4_pmd_pmem,pos));
		pfn_dram = pte_pfn(*pte_offset_kernel(&ext4_pmd_dram,pos));
		if(pfn_pmem != pfn_dram){
						ret=-1;
						return ret;
		}
		pos+=PAGE_SIZE;
	}
	return 0;
}

pmd_t * ext4_daxvm_page_walk(struct vm_fault *vmf){
	
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

bool ext4_daxvm_file_mkwrite(struct vm_fault *vmf){
	
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
	
    if (pmd_none(*pmd)) {
      break;
    }
		*pmd=pmd_mkwrite(pmd_mkdirty(*pmd));
		
		if((vmf->address&PMD_MASK)==address){
			if(is_pmd_daxvm(*pmd) && !pmd_devmap(*pmd))
				ret = pte_present(*pte_offset_kernel(pmd,vmf->address));
      else if (pmd_devmap(*pmd))				ret=1;
		}

		address+=PMD_SIZE; 
	}
out: 
	return ret;
}


bool ext4_daxvm_set_pmd(struct vm_area_struct *vma, pmd_t *pmd, unsigned long address, unsigned long pgoff, 
					bool persistent, bool wrprotect, struct ext4_inode_info *sih, loff_t size){

	pmd_t ext4_pmd={0};
	spinlock_t *ptl;
	bool ret = 0;
	struct mm_struct *mm=vma->vm_mm;
	bool huge_page = 0;	
	unsigned long pfn;


	if(size==PMD_SIZE)
		huge_page=1;

	if(!pmd) {
    goto out;
  }

  // the persistent part is for the migration due to TLB misses, we could not figure out what happens with pmd when huge page and migration
	if(!pmd_none(*pmd)){
		if(!is_pmd_daxvm(*pmd)){
			goto out;
		}
    if(!persistent){
      goto retry; //migrations for tlb purposes
    }
		ptl = pmd_lock(mm, pmd);
		goto already_set;
	}

retry:
	if(sih->ppgd && persistent)
			ext4_pmd = ext4_get_persistent_pmd(sih, (round_down(pgoff, PMD_SIZE>>PAGE_SHIFT))<<PAGE_SHIFT, &huge_page);
  

	if (sih->vpgd && pmd_none(ext4_pmd))
			ext4_pmd = ext4_get_volatile_pmd(sih, (round_down(pgoff, PMD_SIZE>>PAGE_SHIFT))<<PAGE_SHIFT, &huge_page);

	if(pmd_none(ext4_pmd)){
    if(!ret) pr_crit("None %llu %llu\n", sih->vfs_inode.i_ino, pgoff);
		goto out;
	}
		
	ptl = pmd_lock(mm, pmd);
	
 	//huge page promotion
	if(huge_page && __transparent_hugepage_enabled(vma)){ 
		pfn = pte_pfn(*pte_offset_kernel(&ext4_pmd,round_down(address, PMD_SIZE)));
		ext4_pmd = pmd_mkhuge(pfn_pmd(pfn, vma->vm_page_prot));
		ext4_pmd = pmd_mkdevmap(ext4_pmd);
    if(vma->vm_flags & VM_DAXVM_EPHEMERAL)
    		ext4_pmd=pmd_mk_daxvm_ephemeral(ext4_pmd);
		goto set;
	}

	ext4_pmd = pmd_mk_daxvm(ext4_pmd);
  if(vma->vm_flags & VM_DAXVM_EPHEMERAL)
    		ext4_pmd=pmd_mk_daxvm_ephemeral(ext4_pmd);


set:
	set_pmd_at(mm, round_down(address,PMD_SIZE), pmd, ext4_pmd);

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
    else ret = 0;

    if(!ret) pr_crit("Not found! %llu %llu\n", sih->vfs_inode.i_ino, pgoff);
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

int ext4_daxvm_get_pfn(struct inode *inode, loff_t pos, size_t size, pfn_t *pfn){
  struct ext4_inode_info *sih = EXT4_I(inode);
	pmd_t ext4_pmd;
	bool huge_page = 0;

	if (sih->ppgd){
	  if(size == PMD_SIZE) huge_page=1;
		ext4_pmd = ext4_get_persistent_pmd(sih, round_down(pos, PMD_SIZE), &huge_page);
	}
	else if (sih->vpgd) {
	  if(size == PMD_SIZE) huge_page=1;
		ext4_pmd = ext4_get_volatile_pmd(sih, round_down(pos, PMD_SIZE), &huge_page);
	}	
	else {
    		pr_crit("Unfortunately we somehow have missed a file\n");
				return -1;
	}

	if(pmd_none(ext4_pmd) || !pte_present(*pte_offset_kernel(&ext4_pmd,pos))){
    		return -1;
	}

	if ((size == PMD_SIZE) && !huge_page) {
		return -1;
	}

	*pfn = pfn_to_pfn_t(pte_pfn(*pte_offset_kernel(&ext4_pmd,pos)));
	return 0;
}

bool ext4_daxvm_fault(struct vm_fault *vmf, struct inode *inode){

	bool wrprotect = 0; 
	int ret = 0;
  	struct ext4_inode_info *sih = EXT4_I(inode);
	loff_t size = PMD_SIZE;

	if (!((vmf->vma->vm_flags & (VM_WRITE|VM_SHARED))==(VM_WRITE|VM_SHARED)) || (!(vmf->flags & FAULT_FLAG_WRITE)))
		wrprotect=1;

	ret = ext4_daxvm_set_pmd(vmf->vma, ext4_daxvm_page_walk(vmf), vmf->address, vmf->pgoff, 1, wrprotect, sih, size);
	return ret;	
}


//from pmem to dram (currently only when high tlb miss overhead is detected)
void ext4_daxvm_migrate_pgtables(handle_t *handle, struct mm_struct *mm, bool persist){
  	struct vm_area_struct *vma;
		struct super_block *sb;
  	struct inode *inode;
  	struct ext4_inode_info *sih;
  	//from pmem to dram (currently only when high tlb miss overhead is detected)
  	if(!persist){
        down_write(&mm->mmap_sem);
    		for (vma = mm->mmap; vma; vma = vma->vm_next) {
      			if(vma->vm_flags & VM_DAXVM && (vma->vm_end-vma->vm_start) > ext4_daxvm_pmem_threshold){
        			inode = vma->vm_file->f_mapping->host;
							sb = inode->i_sb;
        			sih = EXT4_I(inode);
	      			if(!sih->vpgd)
        				ext4_migrate_pmem_to_dram_tables(inode, sih, (vma->vm_end-vma->vm_start)>>PAGE_SHIFT);
        			zap_page_range(vma,vma->vm_start, vma->vm_end-vma->vm_start);
        			ext4_daxvm_attach_volatile_tables(vma,inode); 
      			}   
    		}
        up_write(&mm->mmap_sem);
  	}
}

void ext4_daxvm_unmap_all_dax(struct mm_struct *mm){
  	struct vm_area_struct *vma;
		struct super_block *sb;
  	struct inode *inode;
  	//from pmem to dram (currently only when high tlb miss overhead is detected)
    down_write(&mm->mmap_sem);
    for (vma = mm->mmap; vma; vma = vma->vm_next) {
      if(vma->vm_file && IS_DAX(vma->vm_file->f_mapping->host)){
        			zap_page_range(vma,vma->vm_start, vma->vm_end-vma->vm_start);
      			}   
    	}
    up_write(&mm->mmap_sem);
}

//if someon maps using the dram tables will fault in the next accesses and attach gradually the new pmem ones 
void ext4_daxvm_migrate_file_pgtables(handle_t * handle, struct inode *inode,  bool persist){
  struct ext4_inode_info *sih;
	struct vm_area_struct *vma;
	struct address_space *mapping;
	struct super_block *sb = inode->i_sb;
  sih = EXT4_I(inode);
  if(persist) {
		if(!sih->ppgd){
 			ext4_migrate_dram_to_pmem_tables(handle, inode, sb, sih, inode->i_size>>PAGE_SHIFT);
	 }
	}
}

void ext4_daxvm_attach_tables(struct vm_area_struct *vma, struct inode *inode){

	unsigned long pgoff = vma->vm_pgoff;
	unsigned long address = vma->vm_start;
	unsigned long end = vma->vm_end;
	struct mm_struct *mm = vma->vm_mm;
  struct ext4_inode_info *sih = EXT4_I(inode);
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	bool wrprotect=0; 
	int ret;
	loff_t size = PAGE_SIZE;

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

		ret = ext4_daxvm_set_pmd(vma, pmd, address, pgoff, 1, wrprotect, sih, size); 
    if(!ret) {
			break;
		}	
		address+=PMD_SIZE; 
		pgoff+=(PMD_SIZE>>PAGE_SHIFT); 
	}
out: 
	return;
}

void ext4_daxvm_attach_volatile_tables(struct vm_area_struct *vma, struct inode *inode){

	unsigned long pgoff = vma->vm_pgoff;
	unsigned long address = vma->vm_start;
	unsigned long end = vma->vm_end;
	struct mm_struct *mm = vma->vm_mm;
  struct ext4_inode_info *sih = EXT4_I(inode);
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	bool wrprotect=0; 
	int ret;
	loff_t size = PAGE_SIZE;

	wrprotect=1;

  if ((address + PMD_SIZE) > end){
		 BUG();
		 goto out;	
	}

	while (address < end){
    pgd = pgd_offset(mm, address);
		if(!pgd) break;
			
    p4d = p4d_offset(pgd, address);
		if (!p4d) break;

		pud = pud_offset(p4d, address);
		if (!pud) break;

		pmd = pmd_offset(pud, address);
		if (!pmd) break;

    if ((end-address) >= PMD_SIZE)
			size=PMD_SIZE;
		
		ret = ext4_daxvm_set_pmd(vma, pmd, address, pgoff, 0, wrprotect, sih, size); 
    if(!ret) {
			break;
		}	
next:
		address+=PMD_SIZE; 
		pgoff+=(PMD_SIZE>>PAGE_SHIFT); 
	}
out: 
	return;
}

void ext4_daxvm_build_tables(handle_t *handle, struct inode *inode, unsigned long num_pages, ext4_fsblk_t m_pblk, ext4_lblk_t m_lblk, bool persist){
      	
	struct super_block *sb = inode->i_sb;
  	struct ext4_sb_info *sbi = EXT4_SB(sb);
	struct ext4_inode_info *sih = EXT4_I(inode);
	int ret=0;	

  if(persist){
    ret = ext4_update_persistent_page_table (handle, inode, sb, sih, num_pages, PAGE_SHARED_EXEC,pfn_to_pfn_t((sbi->dax_phys_addr + (m_pblk<<PAGE_SHIFT))>>PAGE_SHIFT), m_lblk);
    BUG_ON(!sih->ppgd);
  }
  else
    ret = ext4_update_volatile_page_table (inode, sih, num_pages, PAGE_SHARED_EXEC,pfn_to_pfn_t((sbi->dax_phys_addr + (m_pblk<<PAGE_SHIFT))>>PAGE_SHIFT), m_lblk);
}


void ext4_daxvm_delete_tables( handle_t *handle, struct inode*inode, struct super_block *sb, unsigned long start, unsigned long end, bool delete){

	struct ext4_inode_info *sih = EXT4_I(inode);
	
	if(start==end) return;
	
	if(sih->ppgd && delete){
    		if (sih->ppt_level==PTE_LEVEL) {
          if(end > (start + PMD_SIZE)) end=PMD_SIZE;
	      			ext4_clear_persistent_pte(handle, inode, sb, sih, ((pte_t*)sih->ppgd) + pte_index(start), start, end);
   	 	  }
    		else if (sih->ppt_level==PMD_LEVEL) {
          if(end > (start + PUD_SIZE)) end=PUD_SIZE;
	      			ext4_clear_persistent_pmd(handle, inode, sb, sih, ((pmd_t*)sih->ppgd) + pmd_index(start), start, end);
    		}
    		else if(sih->ppt_level==PUD_LEVEL) {
          if(end > (start + PGDIR_SIZE)) end=PGDIR_SIZE;
	     			ext4_clear_persistent_pud(handle, inode, sb, sih, ((pud_t*)sih->ppgd) + pud_index(start), start, end);
    		}
    		else if(sih->ppt_level==PGD_LEVEL) {
	      		ext4_clear_persistent_page_tables(handle, inode, sb, sih, ((pgd_t*)sih->ppgd), start, end);
    		}

    		if(start == 0) {
    			sih->ppgd=NULL;
    			sih->ppt_ceiling=0; 
    			sih->ppt_level=4;
   	 	}		
 }
 if(sih->vpgd ){
    		if (sih->vpt_level==PTE_LEVEL) {
          if(end > (start + PMD_SIZE)) end=PMD_SIZE;
	      			ext4_clear_volatile_pte(inode, sih, ((pte_t*)sih->vpgd) + pte_index(start), start, end);
   	 	  }
    		else if (sih->vpt_level==PMD_LEVEL) {
          if(end > (start + PUD_SIZE)) end=PUD_SIZE;
	      			ext4_clear_volatile_pmd(inode, sih, ((pmd_t*)sih->vpgd) + pmd_index(start), start, end);
    		}
    		else if(sih->vpt_level==PUD_LEVEL) {
          if(end > (start + PGDIR_SIZE)) end=PGDIR_SIZE;
	     			ext4_clear_volatile_pud(inode, sih, ((pud_t*)sih->vpgd) + pud_index(start), start, end);
    		}
    		else if(sih->vpt_level==PGD_LEVEL) {
	      		ext4_clear_volatile_page_tables(inode, sih, ((pgd_t*)sih->vpgd), start, end);
    		}

    		if(start == 0) {
    			sih->vpgd=NULL;
    			sih->vpt_ceiling=0; 
    			sih->vpt_level=4;
   	 	}		
 }

}

void ext4_daxvm_delete_volatile_tables( handle_t *handle, struct inode*inode, unsigned long start, unsigned long end, bool delete){

	struct ext4_inode_info *sih = EXT4_I(inode);
	
	if(start==end) return;
	
 if(sih->vpgd ){
    		if (sih->vpt_level==PTE_LEVEL) {
          if(end > (start + PMD_SIZE)) end=PMD_SIZE;
	      			ext4_clear_volatile_pte(inode, sih, ((pte_t*)sih->vpgd) + pte_index(start), start, end);
   	 	  }
    		else if (sih->vpt_level==PMD_LEVEL) {
          if(end > (start + PUD_SIZE)) end=PUD_SIZE;
	      			ext4_clear_volatile_pmd(inode, sih, ((pmd_t*)sih->vpgd) + pmd_index(start), start, end);
    		}
    		else if(sih->vpt_level==PUD_LEVEL) {
          if(end > (start + PGDIR_SIZE)) end=PGDIR_SIZE;
	     			ext4_clear_volatile_pud(inode, sih, ((pud_t*)sih->vpgd) + pud_index(start), start, end);
    		}
    		else if(sih->vpt_level==PGD_LEVEL) {
	      		ext4_clear_volatile_page_tables(inode, sih, ((pgd_t*)sih->vpgd), start, end);
    		}

    		if(start == 0) {
    			sih->vpgd=NULL;
    			sih->vpt_ceiling=0; 
    			sih->vpt_level=4;
   	 	}		
 }

}


SYSCALL_DEFINE1(daxvm_migrate_to_dram, unsigned long, pid){
	struct task_struct* t;
	t = find_task_by_vpid(pid);
	struct mm_struct *mm = t->mm;
	ext4_daxvm_migrate_pgtables(NULL,mm, 0);
}

SYSCALL_DEFINE1(daxvm_unmap, unsigned long, pid){
	struct task_struct* t;
	t = find_task_by_vpid(pid);
	struct mm_struct *mm = t->mm;
	ext4_daxvm_unmap_all_dax(mm);
}
