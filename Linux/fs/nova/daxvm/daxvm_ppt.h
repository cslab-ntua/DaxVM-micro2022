#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/llist.h>
#include <asm/page.h>		
#include <linux/rbtree.h>
#include <linux/overflow.h>
#include <linux/mm.h>
#include "../nova.h"

#define PTE_LEVEL 3 	//files <2M 
#define PMD_LEVEL 2	  //files <1G
#define PUD_LEVEL 1 	//files <512G
#define PGD_LEVEL 0 
#define CACHELINE_SIZE  (64)

extern int support_clwb;
extern int persist;

extern pmd_t nova_get_persistent_pmd(struct nova_inode_info_header *sih, unsigned long addr, bool *huge);
extern int nova_update_persistent_page_table( struct inode *inode, struct super_block*sb, struct nova_inode_info_header *sih, unsigned long count, pgprot_t prot, pfn_t pfn, unsigned long pgoff);
extern void nova_clear_persistent_pte( struct super_block*sb, struct nova_inode_info_header *sih, pte_t *ptep, unsigned long addr, unsigned long end);
extern void nova_clear_persistent_pmd( struct super_block*sb, struct nova_inode_info_header *sih, pmd_t *pmdp, unsigned long addr, unsigned long end);
extern void nova_clear_persistent_pud( struct super_block*sb, struct nova_inode_info_header *sih, pud_t *pudp, unsigned long addr, unsigned long end);
extern void nova_clear_persistent_page_tables( struct super_block*sb, struct nova_inode_info_header *sih, pgd_t *pgd, unsigned long addr, unsigned long end);
int nova_migrate_dram_to_pmem_tables( struct inode *inode, struct super_block*sb, struct nova_inode_info_header *sih, unsigned long count);


int __p_p4d_alloc_nova( struct inode *inode, pgd_t *pgd, unsigned long address);
p4d_t* __r_p_p4d_alloc_nova( struct inode *inode, pgd_t *pgd, unsigned long address);
int __p_pud_alloc_nova( struct inode *inode, p4d_t *p4d, unsigned long address);
pud_t* __r_p_pud_alloc_nova( struct inode *inode, p4d_t *p4d, unsigned long address);
int __p_pmd_alloc_nova( struct inode *inode, pud_t *pud, unsigned long address);
pmd_t* __r_p_pmd_alloc_nova( struct inode *inode, pud_t *pud, unsigned long address);
int __p_pte_alloc_nova( struct inode *inode, pmd_t *pmd, unsigned long address);
pte_t* __r_p_pte_alloc_nova( struct inode *inode, pmd_t *pmd, unsigned long address);
void *nova_pt_alloc( struct inode *inode);

static inline pte_t *persistent_pte_alloc_nova( struct inode *inode, pmd_t *pmd, unsigned long address)
{
	return (unlikely(pmd_none(*pmd)) && __p_pte_alloc_nova(inode,pmd, address)) ? NULL : pte_offset_kernel(pmd, address);
}

static inline pmd_t *persistent_pmd_alloc_nova( struct inode *inode, pud_t *pud, unsigned long address)
{
	return (unlikely(pud_none(*pud)) && __p_pmd_alloc_nova(inode, pud, address))? NULL: pmd_offset(pud, address);
}

static inline pud_t *persistent_pud_alloc_nova( struct inode *inode, p4d_t *p4d, unsigned long address)
{
	return (unlikely(p4d_none(*p4d)) && __p_pud_alloc_nova(inode,p4d, address)) ? NULL : pud_offset(p4d, address);
}

static inline p4d_t *persistent_p4d_alloc_nova( struct inode *inode, pgd_t *pgd, unsigned long address)
{
  return (unlikely(pgd_none(*pgd)) && __p_p4d_alloc_nova(inode, pgd, address)) ? NULL : p4d_offset(pgd, address);
}

static inline void persistent_pmd_populate_nova(pmd_t *pmd, pte_t *pte)
{
  set_pmd(pmd, __pmd(__pa(pte) | _PAGE_TABLE));
	if(persist) nova_flush_buffer(pmd, sizeof(pmd_t), 0);
}

static inline void persistent_pud_populate_nova(pud_t *pud, pmd_t *pmd)
{
  set_pud(pud, __pud(_PAGE_TABLE | __pa(pmd)));
	if(persist) nova_flush_buffer(pud, sizeof(pud_t), 0);
}

static inline void persistent_p4d_populate_nova(p4d_t *p4d, pud_t *pud)
{
  set_p4d(p4d, __p4d(_PAGE_TABLE | __pa(pud)));
	if(persist) nova_flush_buffer(p4d, sizeof(p4d_t), 0);
}

static inline void persistent_pgd_populate_nova(pgd_t *pgd, p4d_t *p4d)
{
  if (!pgtable_l5_enabled())
    return;
  set_pgd(pgd, __pgd(_PAGE_TABLE | __pa(p4d)));
  if(persist) nova_flush_buffer(pgd, sizeof(pgd_t), 0);
}

