

//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>


int checkAddress(struct mm_struct *mm, int addr)
{
 int include=0;
 struct vm_area_struct *pvma= mm->mmap;
 
 if(mm->mmap == NULL)
     return 0;


 while (pvma!=NULL)
 {
     if (pvma->vm_id==0)
     {
	    if (addr>=pvma->vm_start&&addr<=pvma->sbrk)
        {
            include=1;
            break;
        }
     }
	 else
     {
         if (addr<=pvma->vm_start&&addr>=pvma->sbrk)
         {
             include=1;
             break;
         }
	}
     pvma = pvma->vm_next;
 }
 return !include;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
 
  if (checkAddress(mm,addr))
  {
	printf("Invalid\n");
	return -1;
  }

  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  MEMPHY_read(caller->mram,phyaddr, data);

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
    if (checkAddress(mm,addr))
    {
      printf("Invalid\n");
      return -1;
    }
  
    int pgn = PAGING_PGN(addr);
    int off = PAGING_OFFST(addr);
    int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
    if(pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1; /* invalid page access */


    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

    MEMPHY_write(caller->mram,phyaddr, value);

     return 0;
}








/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct rg_elmt)
{
    int vmaid=rg_elmt.vmaid;
    if (((rg_elmt.rg_start >= rg_elmt.rg_end) && (vmaid == 0) && (rg_elmt.rg_start!=-1))
        || ((rg_elmt.rg_start <= rg_elmt.rg_end) && (vmaid == 1))
        || ((rg_elmt.rg_start==-1 ) && (rg_elmt.rg_end<=0)))
    {
	return -1;
    }
    struct vm_rg_struct *newrg= malloc(sizeof(struct vm_rg_struct));
    newrg->rg_start=rg_elmt.rg_start;
    newrg->rg_end=rg_elmt.rg_end;
    newrg->vmaid=rg_elmt.vmaid;

    struct vm_area_struct *cur_vma= get_vma_by_num(mm,rg_elmt.vmaid);
    if (cur_vma==NULL) return -1;   //warning control

    struct vm_rg_struct *rg_node = cur_vma->vm_freerg_list;
    newrg->rg_next = rg_node;

  /* Enlist the new region to the start of list */
    cur_vma->vm_freerg_list = newrg;
    return 0;
}

/*get_vma_by_num - get vm area by numID
 *@mm: memory region ->vm area struct
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma= mm->mmap;

  if(mm->mmap == NULL)
    return NULL;

  int vmait = 0;
  
  while (vmait < vmaid)
  {
    if(pvma == NULL)
	  return NULL;

    vmait++;
    pvma = pvma->vm_next;
  }
  //return vmait>=vmaid
  return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if(rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory (rg_start,rg_end]
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{

    if (caller->mm->symrgtbl[rgid].rg_start!=caller->mm->symrgtbl[rgid].rg_end){
        return -1;
    }

    struct vm_rg_struct rgnode;

    /* TODO: commit the vmaid */
    rgnode.vmaid=vmaid;



    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
    {
         caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
         caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
         caller->mm->symrgtbl[rgid].vmaid = rgnode.vmaid;
         if (vmaid==0)
         {
             for (int  addr=caller->mm->symrgtbl[rgid].rg_start+1;
             addr<=caller->mm->symrgtbl[rgid].rg_end;addr++)
             {
                 pg_setval(caller->mm,addr,'\0',caller);
             }
         }
         else
         {
             for (int addr=caller->mm->symrgtbl[rgid].rg_start-1;
             addr>=caller->mm->symrgtbl[rgid].rg_end;addr--)
             {
                 pg_setval(caller->mm,addr,'\0',caller);
             }
         }
          printf("ALLOC SUCCESS %d %d\n", 
                  caller->mm->symrgtbl[rgid].rg_start,
                  caller->mm->symrgtbl[rgid].rg_end);

         *alloc_addr = (int) rgnode.rg_start;
         return 0;
    }
    /* TODO: get_free_vmrg_area FAILED handle the region management (Fig.6)*/
    /* TODO retrive current vma if needed, current comment out due to compiler redundant warning*/
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    
    int inc_sz =size;
   printf("SIZE %d\n",inc_sz);
    int inc_limit_ret=0;
    /* TODO retrive old_sbrk if needed, current comment out due to compiler redundant warning*/
    int old_sbrk = (int) cur_vma->sbrk;

    if (vmaid == 0)
    {
#ifdef MM_PAGING_HEAP_GODOWN
        if (cur_vma->vm_end==0)
            old_sbrk=-1;
        if (old_sbrk + size >= caller->vmemsz)
        {
            return -1;
        }
#endif
        if (old_sbrk + size > (int)cur_vma->vm_end)
        {
            inc_vma_limit(caller, vmaid, inc_sz, &inc_limit_ret);
            if (inc_limit_ret == 0)
            {
                return -1;
            }
        }
    }
    else
    { // vmaid = 1
#ifdef MM_PAGING_HEAP_GODOWN
        if (cur_vma->vm_end==caller->vmemsz-1)
            old_sbrk=caller->vmemsz;

#endif

        if (old_sbrk - size < 0)
        {
            return -1;
        }

        if (old_sbrk - size < (int)cur_vma->vm_end)
        {
            inc_vma_limit(caller, vmaid, inc_sz, &inc_limit_ret);
            if (inc_limit_ret == 0)
            {
                return -1;
            }
        }
    }


  /* TODO INCREASE THE LIMIT
   * inc_vma_limit(caller, vmaid, inc_sz)
   */

    if (vmaid == 0) {
        caller->mm->symrgtbl[rgid].rg_start = old_sbrk ;
        caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
        cur_vma->sbrk = old_sbrk + size; //(old_sbrk,old_sbrk+size]
    }
    else
    { // vmaid = 1
#ifdef MM_PAGING_HEAP_GODOWN
        caller->mm->symrgtbl[rgid].rg_start = old_sbrk ;
        caller->mm->symrgtbl[rgid].rg_end = old_sbrk - size;
        cur_vma->sbrk = old_sbrk - size; //[old_sbrk-size,old_sbrk)
#endif
    }
    caller->mm->symrgtbl[rgid].vmaid = vmaid;
    if (vmaid==0)
    {
        for (int addr=caller->mm->symrgtbl[rgid].rg_start+1;
        addr<=caller->mm->symrgtbl[rgid].rg_end;addr++)
        {
            pg_setval(caller->mm,addr,'\0',caller);
        }
    }
    else
    {
        for (int addr=caller->mm->symrgtbl[rgid].rg_start-1;
        addr>=caller->mm->symrgtbl[rgid].rg_end;addr--)
        {
            pg_setval(caller->mm,addr,'\0',caller);
        }
    }
    *alloc_addr = (int) caller->mm->symrgtbl[rgid].rg_start;
    printf("ALLOC SUCCESS %d %d\n",
            caller->mm->symrgtbl[rgid].rg_start,
            caller->mm->symrgtbl[rgid].rg_end);
    return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __free(struct pcb_t *caller, int rgid)
{
  struct vm_rg_struct rgnode;

  // Dummy initialization for avoding compiler dummay warning
  // in incompleted TODO code rgnode will overwrite through implementing
  // the manipulation of rgid later

  if(rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
    return -1;

  rgnode= caller->mm->symrgtbl[rgid];

  if (rgnode.rg_start==rgnode.rg_end)
      return -1; //dummy,size 0 region,  freed region

  struct vm_area_struct *cur_vma= get_vma_by_num(caller->mm,rgnode.vmaid);

  if (cur_vma!=NULL)
  {
      int haveMerged=0;
      struct vm_rg_struct *vmFreergList=cur_vma->vm_freerg_list;
      while (vmFreergList!=NULL && vmFreergList->vmaid==rgnode.vmaid)
      {
          if (vmFreergList->rg_start==rgnode.rg_end)
          {
              rgnode.rg_end=vmFreergList->rg_end;
              /*remove current node*/
              /*Clone next rg node */
              struct vm_rg_struct *nextrg = vmFreergList->rg_next;
	
              if (nextrg != NULL)
              {
                  vmFreergList->rg_start = nextrg->rg_start;
                  vmFreergList->rg_end = nextrg->rg_end;
                  vmFreergList->rg_next = nextrg->rg_next;
                  free(nextrg);
              }
              else
              { /*End of free list */
                  vmFreergList->rg_start = vmFreergList->rg_end;	//dummy, size 0 region
                  vmFreergList->rg_next = NULL;
              }
              haveMerged=1;
          }
          else if (vmFreergList->rg_end==rgnode.rg_start)
          {
              rgnode.rg_start=vmFreergList->rg_start;
              /*Remove current node */
              /*Clone next rg node */
              struct vm_rg_struct *nextrg = vmFreergList->rg_next;
              if (nextrg != NULL)
              {
                  vmFreergList->rg_start = nextrg->rg_start;
                  vmFreergList->rg_end = nextrg->rg_end;
                  vmFreergList->rg_next = nextrg->rg_next;
                  free(nextrg);
              }
              else
              { /*End of free list */
                  vmFreergList->rg_start = vmFreergList->rg_end;	//dummy, size 0 region
                  vmFreergList->rg_next = NULL;
              }
              haveMerged=1;
          }
          vmFreergList=vmFreergList->rg_next;
          if (vmFreergList==NULL)
          {
              if (haveMerged==0) break;
              else
              {
                  haveMerged=0;
                  vmFreergList=cur_vma->vm_freerg_list;
              }
          }
      }
  }

  /* TODO: Manage the collect freed region to freerg_list */
  enlist_vm_freerg_list(caller->mm, rgnode);
  caller->mm->symrgtbl[rgid].rg_start = 0;
  caller->mm->symrgtbl[rgid].rg_end = 0;

  return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr;
  /* By default using vmaid = 0 data*/
  return __alloc(proc, 0, reg_index, size, &addr);
}

/*pgmalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify vaiable in symbole table)
 */
int pgmalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{

  int addr;
  return __alloc(proc, 1, reg_index, size, &addr);
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
   return __free(proc, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{

  if (pgn>=PAGING_MAX_PGN)
     return -1;
  uint32_t pte = mm->pgd[pgn];

  if (!PAGING_PTE_PAGE_PRESENT(pte))
  { /* Page is not online, make it actively living */
    int vicpgn, swpfpn;
    int vicfpn;
    uint32_t vicpte;
	
    int tgtfpn = PAGING_PTE_SWP(pte);//the target frame storing our variable
    int index_of_mswp= PAGING_PTE_SWPTYP(pte);
    int fpn_temp;
    // have free frame

    if (MEMPHY_get_freefp(caller->mram, &fpn_temp) == 0)
    {
        __swap_cp_page(caller->mswp[index_of_mswp], tgtfpn,
                       caller->mram, fpn_temp);
        pte_set_fpn(&mm->pgd[pgn], fpn_temp);
        MEMPHY_del_usedfp(caller->mswp[index_of_mswp],tgtfpn);
        MEMPHY_put_freefp(caller->mswp[index_of_mswp],tgtfpn);
        MEMPHY_put_usedfp(caller->mram,fpn_temp);
    }
    else
    {
        if (find_victim_page(caller->mm, &vicpgn)<0)
        {
            return -1;
        }
        vicpte = mm->pgd[vicpgn];
        vicfpn = PAGING_PTE_FPN(vicpte);
#ifdef SYNC
        pthread_mutex_lock(&MEM_in_use);
#endif
        /* Get free frame in MEMSWP */
        int index_of_active_mswp;
        for (index_of_active_mswp = 0; index_of_active_mswp < PAGING_MAX_MMSWP;
        index_of_active_mswp++)
        {
            if ((struct memphy_struct *)&caller->mswp[index_of_active_mswp] == caller->active_mswp)
                break;
        }
        /* Get free frame in MEMSWP */
        if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0)
        {
            int i;
            for (i = 0; i < PAGING_MAX_MMSWP; i++)
            {
                if (MEMPHY_get_freefp(caller->mswp[i], &swpfpn) == 0)
                {
                    caller->active_mswp = (struct memphy_struct *)&caller->mswp[i];
                    index_of_active_mswp = i;
                    break;
                }
            }
            if (i == PAGING_MAX_MMSWP)
            {
#ifdef SYNC
            pthread_mutex_unlock(&MEM_in_use);
#endif
                return -1;
            }
        }
#ifdef SYNC
        pthread_mutex_unlock(&MEM_in_use);
#endif
        __swap_cp_page(caller->mram,vicfpn,
                       caller->active_mswp, swpfpn);

        pte_set_swap(&caller->mm->pgd[vicpgn], index_of_active_mswp, swpfpn);

        __swap_cp_page(caller->mswp[index_of_mswp], tgtfpn,
                       caller->mram, vicfpn);
        pte_set_fpn(&mm->pgd[pgn], vicfpn);

        MEMPHY_del_usedfp(caller->mswp[index_of_mswp],tgtfpn);
        MEMPHY_put_freefp(caller->mswp[index_of_mswp],tgtfpn);
        MEMPHY_put_usedfp(caller->active_mswp,swpfpn);
    }
    /* TODO: Play with your paging theory here */
    enlist_pgn_node(&caller->mm->fifo_pgn,pgn);
  }
  *fpn = PAGING_PTE_FPN(mm->pgd[pgn]);
  return 0;
}


int __read(struct pcb_t *caller, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  int vmaid = currg->vmaid;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if(currg == NULL || cur_vma == NULL) /* Invalid memory identify */
	  return -1;

  pg_getval(caller->mm, currg->rg_start  + offset, data, caller);

  return 0;
}


/*pgwrite - PAGING-based read a region memory */
int pgread(
		struct pcb_t * proc, // Process executing the instruction
		uint32_t source, // Index of source register
		uint32_t offset, // Source address = [source] + [offset]
		uint32_t destination) 
{
  BYTE data;
  int val = __read(proc, source, offset, &data);

  destination = (uint32_t) data;
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL

#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __write(struct pcb_t *caller, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  int vmaid = currg->vmaid;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  
  if(currg == NULL || cur_vma == NULL) /* Invalid memory identify */
	  return -1;

  pg_setval(caller->mm, currg->rg_start   + offset, value, caller);

  return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(
		struct pcb_t * proc, // Process executing the instruction
		BYTE data, // Data to be wrttien into memory
		uint32_t destination, // Index of destination register
		uint32_t offset)
{
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL

#endif
  MEMPHY_dump(proc->mram);
#endif

  return __write(proc, destination, offset, data);
}


/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;


  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];

    if (!PAGING_PTE_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    }
    else
    {
      fpn = PAGING_PTE_SWP(pte);
      int index_of_mswp= PAGING_PTE_SWPTYP(pte);
      MEMPHY_put_freefp(caller->mswp[index_of_mswp], fpn);
    }
  }

  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct* get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
  struct vm_rg_struct * newrg;
  /* TODO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  int old_sbrk=cur_vma->sbrk;

  newrg = malloc(sizeof(struct vm_rg_struct));
  /* TODO: update the newrg boundary*/
  newrg->vmaid=vmaid;
  if (vmaid == 0)
  {
      if (cur_vma->vm_end==0)
          old_sbrk=-1;
      newrg->rg_start = old_sbrk;
      newrg->rg_end = PAGING_PAGE_ALIGNSZ(newrg->rg_start + size)-1;
  }
  else
  {    //vmaid=1 (heap)
#ifdef MM_PAGING_HEAP_GODOWN
      if (cur_vma->vm_end==caller->vmemsz-1)
          old_sbrk=caller->vmemsz;
      newrg->rg_start =old_sbrk;
      newrg->rg_end = ((old_sbrk -size)/PAGING_PAGESZ)*PAGING_PAGESZ;;
#endif
  }
  return newrg;
}

/*validate_overlap_vm_area  [vm start,vm end]
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
    if (vmaid==0)
    {
        vmastart=vmastart+1;
    }
    else
    {
#ifdef MM_PAGING_HEAP_GODOWN
        vmastart=vmastart-1;
#endif
    }
    struct vm_area_struct *vma = caller->mm->mmap;
  /* TODO validate the planned memory area is not overlapped */
    while (vma !=NULL )
    {
        if (vma->vm_id==vmaid)
        {
            vma=vma->vm_next;
            continue;
        }
        if (vma->vm_end==vma->vm_start)
        {
            vma=vma->vm_next;
            continue;
        }

        if ((vmastart-(int)vma->vm_end>=0)&&(vmastart-(int)vma->vm_start<=0))
            return -1;
        if ((vmastart-(int)vma->vm_end<=0)&&(vmastart-(int)vma->vm_start>=0))
            return -1;
        if ((vmaend-(int)vma->vm_end>=0)&&(vmaend-(int)vma->vm_start<=0))
            return -1;
        if ((vmaend-(int)vma->vm_end<=0)&&(vmaend-(int)vma->vm_start>=0))
            return -1;

        vma=vma->vm_next;
    }
    return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size 
 *@inc_limit_ret: increment limit return
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz, int* inc_limit_ret)
{

    struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));
    newrg->vmaid = vmaid;

    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    int old_sbrk=cur_vma->sbrk;
    int inc_amt ;

    if (vmaid == 0)
    {
        if (cur_vma->vm_end==0)
            old_sbrk=-1;
        int new_vmaend= PAGING_PAGE_ALIGNSZ(old_sbrk + inc_sz) -1 ;
        inc_amt = PAGING_PAGE_ALIGNSZ(-cur_vma->vm_end+(new_vmaend) );
    }
    else
    { // vmaid = 1
#ifdef MM_PAGING_HEAP_GODOWN
         if (cur_vma->vm_end==caller->vmemsz-1)
            old_sbrk=caller->vmemsz;
        int new_vmaend = ((old_sbrk - inc_sz)/PAGING_PAGESZ)*PAGING_PAGESZ;
        inc_amt = PAGING_PAGE_ALIGNSZ(cur_vma->vm_end-(new_vmaend));  //size from vm_end
#endif
    }

    int incnumpage =  inc_amt / PAGING_PAGESZ; // Num pages allocated begin at vm_end
	
    struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);

    int old_end = (int)cur_vma->vm_end;
    /*Validate overlap of obtained region */

    if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0){
        free(newrg);
        free(area);
        return -1; /*Overlap and failed allocation */
    }

	
    /* TODO: Obtain the new vm area based on vmaid */
    if (vmaid == 0)
    {
        cur_vma->vm_end = PAGING_PAGE_ALIGNSZ(old_sbrk + inc_sz) -1 ;
    }
    else
    {
        cur_vma->vm_end = ((old_sbrk - inc_sz)/PAGING_PAGESZ)*PAGING_PAGESZ; ; // to avoid rounding up
    }


    /* May needs provide some physical frames and then map them using Page Table Entry begin at old end*/
    if (vm_map_ram(caller, area->rg_start, area->rg_end,
                   old_end, incnumpage , newrg) < 0)
    {
        cur_vma->vm_end =old_end;
        free(area);
        free(newrg);
        return -1; /* Map the memory to MEMRAM */
    }
    free(area);
    free(newrg);
    *inc_limit_ret=inc_amt;
    return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn) 
{
  struct pgn_t *pg = mm->fifo_pgn;
  if (pg==NULL)
        return -1;
  /* TODO: Implement the theorical mechanism to find the victim page */
  struct pgn_t *prev_Page=NULL;

  while (pg->pg_next!=NULL)
  {
      prev_Page = pg;
      pg = pg->pg_next;
  }

  if (prev_Page==NULL){
      mm->fifo_pgn = NULL;
  }
  else
  {
      prev_Page->pg_next = NULL;
  }

  *retpgn = pg->pgn;
  free(pg);
  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size 
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (cur_vma==NULL)return -1;
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;
  
  while (rgit != NULL && rgit->vmaid == vmaid)
  {
      if ((rgit->rg_start + size <= rgit->rg_end &&vmaid==0)
      ||(rgit->rg_start-size>=rgit->rg_end&&vmaid==1))
    {
      newrg->rg_start = rgit->rg_start;
      if (vmaid==0)
      {
         newrg->rg_end = rgit->rg_start + size;
      }
      else
      {
         newrg->rg_end=rgit->rg_start-size;
      }
      if (rgit->rg_start + size < rgit->rg_end &&vmaid==0)
      {
        rgit->rg_start =rgit->rg_start + size; //=rg_start+size / rg_start-size
      }
      else if (rgit->rg_start-size > rgit->rg_end&&vmaid==1)
      {
        rgit->rg_start =rgit->rg_start - size;
      }
      else
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        { /*End of free list */
          rgit->rg_start = rgit->rg_end;	//dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
      break;
    }
    else
    {
      rgit = rgit->rg_next;	// Traverse next rg
    }
  }
  if(newrg->rg_start == newrg->rg_end)
      return -1;
  return 0;
}

