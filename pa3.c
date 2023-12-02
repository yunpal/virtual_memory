/**********************************************************************
 * Copyright (c) 2020-2023
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "list_head.h"
#include "vm.h"

/**
 * Ready queue of the system
 */
extern struct list_head processes;

/**
 * Currently running process
 */
extern struct process *current;

/**
 * Page Table Base Register that MMU will walk through for address translation
 */
extern struct pagetable *ptbr;

/**
 * TLB of the system.
 */
extern struct tlb_entry tlb[1UL << (PTES_PER_PAGE_SHIFT * 2)];


/**
 * The number of mappings for each page frame. Can be used to determine how
 * many processes are using the page frames.
 */
extern unsigned int mapcounts[];


/**
 * lookup_tlb(@vpn, @rw, @pfn)
 *
 * DESCRIPTION
 *   Translate @vpn of the current process through TLB. DO NOT make your own
 *   data structure for TLB, but should use the defined @tlb data structure
 *   to translate. If the requested VPN exists in the TLB and it has the same
 *   rw flag, return true with @pfn is set to its PFN. Otherwise, return false.
 *   The framework calls this function when needed, so do not call
 *   this function manually.
 *
 * RETURN
 *   Return true if the translation is cached in the TLB.
 *   Return false otherwise
 */
bool lookup_tlb(unsigned int vpn, unsigned int rw, unsigned int *pfn)
{
	int pd_index = vpn / NR_PTES_PER_PAGE;
	int pte_index = vpn % NR_PTES_PER_PAGE;
	for (int i = 0; i < NR_TLB_ENTRIES; i++)
	{
		if (tlb[i].vpn==vpn)
		{
			if (tlb[i].valid == true)
			{
				if (tlb[i].rw == 1)
				{
					if (rw == 1)
					{
						*pfn = ptbr->outer_ptes[pd_index]->ptes[pte_index].pfn;
						return true;
					}
					else
					{
						return false;
					}
				}
				else if (tlb[i].rw == 2)
				{
					if (rw == 2)
					{
						*pfn = ptbr->outer_ptes[pd_index]->ptes[pte_index].pfn;
						return true;
					}
					else
					{
						return false;
					}
				}
				else if (tlb[i].rw == 3)
				{
					if (rw==1|rw==2|rw==3)
					{
						*pfn = ptbr->outer_ptes[pd_index]->ptes[pte_index].pfn;
						return true;
					}
					else
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
	}
}


/**
 * insert_tlb(@vpn, @rw, @pfn)
 *
 * DESCRIPTION
 *   Insert the mapping from @vpn to @pfn for @rw into the TLB. The framework will
 *   call this function when required, so no need to call this function manually.
 *   Note that if there exists an entry for @vpn already, just update it accordingly
 *   rather than removing it or creating a new entry.
 *   Also, in the current simulator, TLB is big enough to cache all the entries of
 *   the current page table, so don't worry about TLB entry eviction. ;-)
 */
void insert_tlb(unsigned int vpn, unsigned int rw, unsigned int pfn)
{
	int pd_index = vpn / NR_PTES_PER_PAGE;
	int pte_index = vpn % NR_PTES_PER_PAGE;
	for (int i = 0; i < NR_TLB_ENTRIES; i++)
	{
		if (tlb[i].valid== false)
		{
			tlb[i].valid = true;
			tlb[i].rw = rw;
			tlb[i].vpn = vpn;
			tlb[i].pfn = pfn;
			tlb[i].private = 0;
			break;
		}
	}
}


/**
 * alloc_page(@vpn, @rw)
 *
 * DESCRIPTION
 *   Allocate a page frame that is not allocated to any process, and map it
 *   to @vpn. When the system has multiple free pages, this function should
 *   allocate the page frame with the **smallest pfn**.
 *   You may construct the page table of the @current process. When the page
 *   is allocated with ACCESS_WRITE flag, the page may be later accessed for writes.
 *   However, the pages populated with ACCESS_READ should not be accessible with
 *   ACCESS_WRITE accesses.
 *
 * RETURN
 *   Return allocated page frame number.
 *   Return -1 if all page frames are allocated.
 */
unsigned int alloc_page(unsigned int vpn, unsigned int rw)
{
	int pd_index = vpn / NR_PTES_PER_PAGE;
	int pte_index = vpn % NR_PTES_PER_PAGE;
	if (vpn < 0 || vpn >= 256)
	{
		return -1;
	}
	else
	{ 
		if (ptbr->outer_ptes[pd_index] == NULL)//이 부분문제였음
		{
			ptbr->outer_ptes[pd_index] = (struct pte_directory*)malloc(1 * sizeof(struct pte_directory));
		}
		ptbr->outer_ptes[pd_index]->ptes[pte_index].valid = true;
		if (rw == 1)
		{
			ptbr->outer_ptes[pd_index]->ptes[pte_index].rw = ACCESS_READ;
		}
		else if (rw == 2)
		{
			ptbr->outer_ptes[pd_index]->ptes[pte_index].rw = ACCESS_WRITE;
		}
		else if (rw == 3)
		{
			ptbr->outer_ptes[pd_index]->ptes[pte_index].rw = 3;
		}
		for (int i = 0; i < 128; i++)
		{
			if (mapcounts[i] == 0)
			{
				ptbr->outer_ptes[pd_index]->ptes[pte_index].pfn = i;
				mapcounts[i]++;
				break;
			}
		}
		//private 아직 설정 안해줌
		ptbr->outer_ptes[pd_index]->ptes[pte_index].private = 0;

		current->pagetable = *ptbr;
		return ptbr->outer_ptes[pd_index]->ptes[pte_index].pfn;
	}
}


/**
 * free_page(@vpn)
 *
 * DESCRIPTION
 *   Deallocate the page from the current processor. Make sure that the fields
 *   for the corresponding PTE (valid, rw, pfn) is set @false or 0.
 *   Also, consider the case when a page is shared by two processes,
 *   and one process is about to free the page. Also, think about TLB as well ;-)
 */
void free_page(unsigned int vpn)
{
	int pd_index = vpn / NR_PTES_PER_PAGE;
	int pte_index = vpn % NR_PTES_PER_PAGE;
	for (int i = 0; i < NR_TLB_ENTRIES; i++)
	{
		tlb[current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn].valid = false;
		tlb[current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn].rw = 0;
		tlb[current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn].vpn = 0;
		tlb[current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn].private = 0;
		tlb[current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn].pfn = 0;
	}
	mapcounts[current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn] --;
	current->pagetable.outer_ptes[pd_index]->ptes[pte_index].valid = false;
	current->pagetable.outer_ptes[pd_index]->ptes[pte_index].rw = 0;
	current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn = 0;
	current->pagetable.outer_ptes[pd_index]->ptes[pte_index].private = 0;
}


/**
 * handle_page_fault()
 *
 * DESCRIPTION
 *   Handle the page fault for accessing @vpn for @rw. This function is called
 *   by the framework when the __translate() for @vpn fails. This implies;
 *   0. page directory is invalid
 *   1. pte is invalid
 *   2. pte is not writable but @rw is for write
 *   This function should identify the situation, and do the copy-on-write if
 *   necessary.
 *
 * RETURN
 *   @true on successful fault handling
 *   @false otherwise
 */
bool handle_page_fault(unsigned int vpn, unsigned int rw)
{
	int pd_index = vpn / NR_PTES_PER_PAGE;
	int pte_index = vpn % NR_PTES_PER_PAGE;
	struct pte tmp_pte;
	tmp_pte = current->pagetable.outer_ptes[pd_index]->ptes[pte_index];
	int suc = 0;
	if (current->pagetable.outer_ptes[pd_index]->ptes[pte_index].private == 1)
	{

		if (mapcounts[tmp_pte.pfn]>1)
		{
			if (rw == 2)
			{
				for (int i = 0; i < 128; i++)
				{
					if (mapcounts[i] == 0)
					{
						current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn = i;
						mapcounts[i]++;
						current->pagetable.outer_ptes[pd_index]->ptes[pte_index].rw = 3;
						current->pagetable.outer_ptes[pd_index]->ptes[pte_index].private = 0;
						suc = 1;
						break;
					}
				}
			}
			for (int i = 0; i < NR_TLB_ENTRIES; i++)
			{
				if (tlb[i].vpn==vpn)
				{
					tlb[i].rw = 3;
					tlb[i].pfn = current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn;
					tlb[i].private = 0;
					break;
				}
			}
			if (suc == 1)
			{
				mapcounts[tmp_pte.pfn]--;
				return true;
			}

		}
		else
		{
			
			if (rw == 2)
			{
				current->pagetable.outer_ptes[pd_index]->ptes[pte_index].rw = 3;
				current->pagetable.outer_ptes[pd_index]->ptes[pte_index].private = 0;
				for (int i = 0; i < NR_TLB_ENTRIES; i++)
				{
					if (tlb[i].vpn == vpn)
					{
						tlb[i].rw = 3;
						tlb[i].private = 0;
						break;
					}
				}
				return true;
			}
		}
	}
	else
	{
		return false;
	}
}


/**
 * switch_process()
 *
 * DESCRIPTION
 *   If there is a process with @pid in @processes, switch to the process.
 *   The @current process at the moment should be put into the @processes
 *   list, and @current should be replaced to the requested process.
 *   Make sure that the next process is unlinked from the @processes, and
 *   @ptbr is set properly.
 *
 *   If there is no process with @pid in the @processes list, fork a process
 *   from the @current. This implies the forked child process should have
 *   the identical page table entry 'values' to its parent's (i.e., @current)
 *   page table. 
 *   To implement the copy-on-write feature, you should manipulate the writable
 *   bit in PTE and mapcounts for shared pages. You may use pte->private for 
 *   storing some useful information :-)
 */
void switch_process(unsigned int pid)
{
	struct process* new;
	int i = 0;
	struct pagetable* new_ptbr;

	list_add(&current->list, &processes);
	list_for_each_entry_reverse(new, &processes, list)
	{
		if (new->pid == pid)
		{
			current=new;
			ptbr = &current->pagetable;
			i = 1;
			list_del_init(&new->list);
			break;
		}
	}
	if (i == 0)
	{
		for (int i = 0; i < NR_TLB_ENTRIES; i++)
		{
			tlb[i].valid = false;
			tlb[i].rw = 0;
			tlb[i].vpn = 0;
			tlb[i].pfn = 0;
			tlb[i].private = 0;
		}
		new = (struct process*)malloc(1 * sizeof(struct process));
		for (int a = 0; a < 16; a++)
		{
			if (current->pagetable.outer_ptes[a] != NULL)
			{
				for (int b = 0; b < 16; b++)
				{
					if (current->pagetable.outer_ptes[a]->ptes[b].valid == true)
					{
						if (new->pagetable.outer_ptes[a] == NULL)
						{
							new->pagetable.outer_ptes[a] = (struct pte_directory*)malloc(1 * sizeof(struct pte_directory));
						}
						new->pagetable.outer_ptes[a]->ptes[b].valid = true;
						if (current->pagetable.outer_ptes[a]->ptes[b].rw == 1)
						{
							new->pagetable.outer_ptes[a]->ptes[b].pfn = current->pagetable.outer_ptes[a]->ptes[b].pfn;
							new->pagetable.outer_ptes[a]->ptes[b].rw = 1;
							if (current->pagetable.outer_ptes[a]->ptes[b].private == 1)
							{
								new->pagetable.outer_ptes[a]->ptes[b].private = 1;
							}
							else
							{
								new->pagetable.outer_ptes[a]->ptes[b].private = 0;
							}
							mapcounts[current->pagetable.outer_ptes[a]->ptes[b].pfn]++;
							
						}
						else if (current->pagetable.outer_ptes[a]->ptes[b].rw == 3)
						{
							current->pagetable.outer_ptes[a]->ptes[b].rw = 1;
							current->pagetable.outer_ptes[a]->ptes[b].private = 1;
							new->pagetable.outer_ptes[a]->ptes[b].rw = 1;
							new->pagetable.outer_ptes[a]->ptes[b].private = 1;
							new->pagetable.outer_ptes[a]->ptes[b].pfn = current->pagetable.outer_ptes[a]->ptes[b].pfn;
							mapcounts[current->pagetable.outer_ptes[a]->ptes[b].pfn]++;
						}
					}
				}
			}
			else
			{
				new->pagetable.outer_ptes[a] = NULL;
			}
		}
		

		new->pid = pid;
		current = new;
		ptbr = &current->pagetable;
		//page talbe이 독립적이지 못함
	}
	else 
	{
		for (int i = 0; i < NR_TLB_ENTRIES; i++)
		{
				tlb[i].valid =false;
				tlb[i].rw = 0;
				tlb[i].vpn = 0;
				tlb[i].pfn =0;
				tlb[i].private = 0;
		}
	}

}
