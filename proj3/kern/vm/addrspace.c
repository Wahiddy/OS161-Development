/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <synch.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */
struct region *create_region(vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable);

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/* Create level one hierarchy of page table */
	paddr_t **pg_table;
	pg_table = kmalloc(PG_TABLE_ONE * sizeof(paddr_t *));
	if (pg_table == NULL) {
		kfree(as);
		return NULL;
	}

	for (int i = 0; i < PG_TABLE_ONE; i++) {
		pg_table[i] = NULL;
	}

	as->head = NULL;
	as->page_table = pg_table;

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *as_copy;

	as_copy = as_create();
	if (as_copy==NULL) {
		return ENOMEM;
	}
	memcpy(as_copy, old, sizeof(struct addrspace));

	struct region *curr_old = old->head;
	struct region *curr_copy = as_copy->head;
	while(curr_old != NULL) {
		/* sus: curr_old->rise is most like 4 KiB. Usually, 1 KiB is passed in and 
		aligned to 4 Kib. So, not sure if passed in 4 KiB, it will stay 4 KiB or
		not */
		int result = as_define_region(as_copy, curr_old->base_addr, curr_old->r_size,
									curr_old->read, curr_old->write, curr_old->exec);
		if (result) {
			panic("error: as_copy: could not allocate region for copy");
			return ENOMEM;
		}
		curr_copy = curr_copy->next;
		curr_old = curr_old->next;
	}

	paddr_t **pg_table_copy;
	pg_table_copy = kmalloc(PG_TABLE_ONE * sizeof(paddr_t *));
	
	for (int i = 0; i < PG_TABLE_ONE; i++) {
		if (old->page_table[i] != NULL) {
			pg_table_copy[i] = kmalloc(PG_TABLE_TWO * sizeof(paddr_t));
			memcpy(pg_table_copy[i], old->page_table[i], sizeof(paddr_t *));

			// for (int j = 0; j < PG_TABLE_TWO; j++) {
			// 	pg_table_copy[i][j] = NO_ENTRY;
			// 	//memcpy(pg_table_copy[i][j], old->page_table[i][j], sizeof(paddr_t));
			// }
		} 
		else {
			pg_table_copy[i] = NULL;
		}
	}

	*ret = as_copy;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/* Go through page table entries and free the physical frames
	 * convert the PFN to a VP and free
	 */
	kfree(as);
	as = NULL;
}

/* Can be directly copied from dumbvm */
void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	/* Copied directly from dumbvm.c 
	 * Align the pages, so checking for overlapping regions is 
	 * simplified
	*/
	memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

	/* Check if region within KUSEG */
	if ((vaddr + memsize) > MIPS_KSEG0) {
		return EFAULT;
	}

	struct region *rg = create_region(vaddr, memsize, readable, writeable, executable);

	if (rg == NULL) {
		return EFAULT;
	}
	
	/* Insert the region into the addrspace */
	rg->next = as->head;
	as->head = rg;
	
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/* Make readonly regions read/write */
	struct region *curr = as->head;
	while (curr != NULL) {
		curr->write = 1;
		curr = curr->next;
	}
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	as_activate();

	struct region *curr = as->head;
	while(curr != NULL) {
		curr->write = curr->ker_write;
		curr = curr->next;
	}

	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	int res = as_define_region(as, *stackptr - PAGE_SIZE * STACK_PAGES, PAGE_SIZE * STACK_PAGES, 1, 1, 0);

	return res;
}

/* Helper functions to create a new region */
struct region *create_region(vaddr_t vaddr, size_t memsize, int readable, int writeable, int executable) {
	struct region *rg = kmalloc(sizeof(struct region));
	if (rg == NULL) {
		return NULL;
	}
	rg->base_addr = vaddr;
	rg->r_size = memsize;
	rg->read = readable;
	rg->write = writeable;
	rg->exec = executable;
	rg->ker_write = writeable;
	rg->next = NULL;
	return rg;
}