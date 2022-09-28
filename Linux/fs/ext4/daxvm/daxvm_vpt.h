#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/llist.h>
#include <asm/page.h>		
#include <linux/rbtree.h>
#include <linux/overflow.h>
#include <linux/mm.h>
#include "../ext4.h"

#define PTE_LEVEL 3 	//files <2M 
#define PMD_LEVEL 2	  //files <1G
#define PUD_LEVEL 1 	//files <512G
#define PGD_LEVEL 0 
#define CACHELINE_SIZE  (64)

extern int support_clwb;
extern int persist;

extern pmd_t ext4_get_volatile_pmd(struct ext4_inode_info *sih, unsigned long addr, bool *huge);
extern int ext4_update_volatile_page_table( struct inode *inode,  struct ext4_inode_info *sih, unsigned long count, pgprot_t prot, pfn_t pfn, unsigned long pgoff);
extern int ext4_migrate_pmem_to_dram_tables(struct inode *inode, struct ext4_inode_info *sih, unsigned long count);
extern void ext4_clear_volatile_pte( struct inode *inode,  struct ext4_inode_info *sih, pte_t *ptep, unsigned long addr, unsigned long end);
extern void ext4_clear_volatile_pmd( struct inode *inode,  struct ext4_inode_info *sih, pmd_t *pmdp, unsigned long addr, unsigned long end);
extern void ext4_clear_volatile_pud( struct inode *inode,  struct ext4_inode_info *sih, pud_t *pudp, unsigned long addr, unsigned long end);
extern void ext4_clear_volatile_page_tables( struct inode *inode,  struct ext4_inode_info *sih, pgd_t *pgd, unsigned long addr, unsigned long end);

int __v_p4d_alloc_ext4( struct inode *inode, pgd_t *pgd, unsigned long address);
p4d_t* __r_v_p4d_alloc_ext4( struct inode *inode, pgd_t *pgd, unsigned long address);
int __v_pud_alloc_ext4( struct inode *inode, p4d_t *p4d, unsigned long address);
pud_t* __r_v_pud_alloc_ext4( struct inode *inode, p4d_t *p4d, unsigned long address);
int __v_pmd_alloc_ext4( struct inode *inode, pud_t *pud, unsigned long address);
pmd_t* __r_v_pmd_alloc_ext4( struct inode *inode, pud_t *pud, unsigned long address);
int __v_pte_alloc_ext4( struct inode *inode, pmd_t *pmd, unsigned long address);
pte_t* __r_v_pte_alloc_ext4( struct inode *inode, pmd_t *pmd, unsigned long address);
void *ext4_vpt_alloc( struct inode *inode);

static inline pte_t *volatile_pte_alloc_ext4( struct inode *inode, pmd_t *pmd, unsigned long address)
{
	return (unlikely(pmd_none(*pmd)) && __v_pte_alloc_ext4(inode,pmd, address)) ? NULL : pte_offset_kernel(pmd, address);
}

static inline pmd_t *volatile_pmd_alloc_ext4( struct inode *inode, pud_t *pud, unsigned long address)
{
	return (unlikely(pud_none(*pud)) && __v_pmd_alloc_ext4(inode, pud, address))? NULL: pmd_offset(pud, address);
}

static inline pud_t *volatile_pud_alloc_ext4( struct inode *inode, p4d_t *p4d, unsigned long address)
{
	return (unlikely(p4d_none(*p4d)) && __v_pud_alloc_ext4(inode,p4d, address)) ? NULL : pud_offset(p4d, address);
}

static inline p4d_t *volatile_p4d_alloc_ext4( struct inode *inode, pgd_t *pgd, unsigned long address)
{
  return (unlikely(pgd_none(*pgd)) && __v_p4d_alloc_ext4(inode, pgd, address)) ? NULL : p4d_offset(pgd, address);
}

static inline void volatile_pmd_populate_ext4(pmd_t *pmd, pte_t *pte)
{
  set_pmd(pmd, __pmd(__pa(pte) | _PAGE_TABLE));
}

static inline void volatile_pud_populate_ext4(pud_t *pud, pmd_t *pmd)
{
  set_pud(pud, __pud(_PAGE_TABLE | __pa(pmd)));
}

static inline void volatile_p4d_populate_ext4(p4d_t *p4d, pud_t *pud)
{
  set_p4d(p4d, __p4d(_PAGE_TABLE | __pa(pud)));
}

static inline void volatile_pgd_populate_ext4(pgd_t *pgd, p4d_t *p4d)
{
  if (!pgtable_l5_enabled())
    return;
  set_pgd(pgd, __pgd(_PAGE_TABLE | __pa(p4d)));
}

