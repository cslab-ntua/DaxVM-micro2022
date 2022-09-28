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

#include "../ext4.h" 
#include "../../daxvm.h" 
#include "daxvm_ppt.h"
#include "daxvm_vpt.h"

pmd_t * ext4_daxvm_page_walk(struct vm_fault *vmf);
bool ext4_daxvm_file_mkwrite(struct vm_fault *vmf);
bool ext4_daxvm_set_pmd(struct vm_area_struct *vma, pmd_t *pmd, unsigned long address, unsigned long pgoff, bool persistent, bool wrprotect, struct ext4_inode_info *sih, loff_t size);
bool ext4_daxvm_fault(struct vm_fault *vmf, struct inode *inode);
void ext4_daxvm_attach_tables(struct vm_area_struct *vma, struct inode *inode);
void ext4_daxvm_attach_volatile_tables(struct vm_area_struct *vma, struct inode *inode);
void ext4_daxvm_delete_tables( handle_t *handle, struct inode*inode, struct super_block *sb, unsigned long start, unsigned long end, bool _delete);
void ext4_daxvm_delete_volatile_tables( handle_t *handle, struct inode*inode, unsigned long start, unsigned long end, bool _delete);
void ext4_daxvm_build_tables(handle_t *handle, struct inode *inode, unsigned long num_pages, ext4_fsblk_t m_pblk, ext4_lblk_t m_lblk, bool persist);
int  ext4_daxvm_get_pfn(struct inode *inode, loff_t pos, size_t size, pfn_t *pfn);
void ext4_daxvm_migrate_pgtables(handle_t *handle, struct mm_struct *mm, bool persist);
void ext4_daxvm_migrate_file_pgtables(handle_t *handle, struct inode *inode, bool persist);
extern unsigned long ext4_daxvm_pmem_threshold;
