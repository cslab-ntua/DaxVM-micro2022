#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/llist.h>
#include <asm/page.h>		/* pgprot_t */
#include <linux/rbtree.h>
#include <linux/overflow.h>
#include <linux/pfn_t.h>
#include <linux/module.h>
#include <linux/buffer_head.h>
#include <linux/cpufeature.h>
#include <asm/pgtable.h>
#include <linux/version.h>

#include "../nova.h" 
#include "../../daxvm.h" 
#include "daxvm_ppt.h"
#include "daxvm_vpt.h"

extern int nova_persistent_page_tables;
extern atomic64_t nova_pmem_pages;
extern atomic64_t nova_dram_pages;
pmd_t * nova_daxvm_page_walk(struct vm_fault *vmf);
bool nova_daxvm_file_mkwrite(struct vm_fault *vmf);
bool nova_daxvm_set_pmd(struct vm_area_struct *vma, pmd_t *pmd, unsigned long address, unsigned long pgoff, bool persistent, bool wrprotect, struct nova_inode_info_header *sih, loff_t size);
bool nova_daxvm_fault(struct vm_fault *vmf, struct inode *inode);
void nova_daxvm_attach_tables(struct vm_area_struct *vma, struct inode *inode);
void nova_daxvm_delete_tables(  struct nova_inode_info_header*sih, struct super_block *sb, unsigned long start, unsigned long end, bool _delete);
void nova_daxvm_build_tables( struct inode *inode, unsigned long num_pages, unsigned long m_pblk, unsigned long m_lblk, bool persist);
int  nova_daxvm_get_pfn(struct inode *inode, loff_t pos, size_t size, pfn_t *pfn);
void nova_daxvm_migrate_pgtables( struct mm_struct *mm, bool persist);
void nova_daxvm_migrate_file_pgtables( struct inode *inode, bool persist);
extern unsigned long nova_daxvm_pmem_threshold;
extern int nova_zeroout;
