#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <spl.h>
#include <proc.h>
#include <synch.h>

/* Page table specific functions */
int insert_pte(struct addrspace *as, vaddr_t pt_index, paddr_t pt_entry);
paddr_t get_pte(struct addrspace *as, vaddr_t pt_index);

/* Region specific functions */
struct region *return_region(struct addrspace *as, vaddr_t faultaddress);
int validate_region(struct addrspace *as, vaddr_t faultaddress, int faulttype);

void vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
     *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */
}


/*
 * Every time tlb miss occurs vm_fault gets called, because of a reason:
 * 1. Access to illegal range of memory or,
 * 2. Write to read only memory or,
 * 3. Accessing valid memory, that is already mapped but tlb is empty or,
 * 4. Accessing valid memory, that is NOT already mapped and needs to be allocated
*/
int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    if (faulttype == VM_FAULT_READONLY) {
        return EFAULT;
    }

    struct addrspace *as = proc_getas();    // Get the current processes addrespace

    if (as == NULL) {
        return ESRCH;
    }

    /* Check page table and load tlb if translation valid */
    paddr_t p_frame = get_pte(as, faultaddress);
    if (p_frame != NO_ENTRY) {
        tlb_refill(faultaddress & PAGE_FRAME, p_frame);     // EntryHi, EntryLo
        return 0;
    }

    /* Check if addr is valid in any region */ 
    if (validate_region(as, faultaddress, faulttype)) {
        return EFAULT;
    }
    
    /* Allocate a physical frame after all checks have been done */
    vaddr_t new_va = alloc_kpages(1);
    if (new_va == 0) {
        return ENOMEM;
    }

    bzero((void *) new_va, PAGE_SIZE);                      // Zero out entries in physical frame
    paddr_t pfn = KVADDR_TO_PADDR(new_va) & PAGE_FRAME;     // Extract only the PFN
    pfn = pfn | TLBLO_VALID;                                // Set valid bit

    struct region *f_region = return_region(as, faultaddress); // Get the region to check permissions
    if (f_region == NULL) {
        return EFAULT;
    }

    /* Set write permissions */  
    if (f_region->write) {
        pfn = pfn | TLBLO_DIRTY;
    }

    /* Insert into page table */
    if (insert_pte(as, faultaddress, pfn)) {
        return EFAULT;
    }

    /* Load the tlb */
    tlb_refill(faultaddress & PAGE_FRAME, pfn);
    return 0;
}

/*
 * SMP-specific functions.  Unused in our UNSW configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

/*
 * Insert an entry into page table when vm_fault occurs.
 * This ONLY handles putting stuff into and creating level two page
 * table entries and errors related to it. Allocating physical frames, zeroing
 * checking if the paddr is valid has to be done before or after. 
 * 
 * Return 0 indicates success
*/
int insert_pte(struct addrspace *as, vaddr_t pt_index, paddr_t pt_entry) {
    uint32_t node1 = get_level_one(pt_index);
    uint32_t node2 = get_level_two(pt_index);

    if (as->page_table[node1] != NULL) {

        /* Page table entry already occupied */
        if (as->page_table[node1][node2] != NO_ENTRY) {
            return EFAULT;
        }

        /* Insert entry no trouble */
        as->page_table[node1][node2] = pt_entry;
        return 0;
    }

    /* Create a level two page table */
    as->page_table[node1] = kmalloc(PG_TABLE_TWO * sizeof(paddr_t));
    if (as->page_table[node1] == NULL) {
        return ENOMEM;
    }

    /* Set all entries to empty for proper checking later */
    for (int i = 0; i < PG_TABLE_TWO; i++) {
        as->page_table[node1][node2] = NO_ENTRY;
    }

    as->page_table[node1][node2] = pt_entry;
    return 0;
}

/*
 * Check if page table index given is even valid.
 * Must check level one, check level two then
 * check if an entry even exists. If entry exists
 * return the entry.
 * 
 * Return 0 indicates no entry was found
 * Otherwise the physical address is returned
 * 
 * Physical address is used to load the tlb after
*/
paddr_t get_pte(struct addrspace *as, vaddr_t pt_index) {
    uint32_t node1 = get_level_one(pt_index);
    uint32_t node2 = get_level_two(pt_index);

    if (as->page_table[node1] == NULL) {
        return NO_ENTRY;
    }

    if (as->page_table[node1][node2] == NO_ENTRY) {
        return NO_ENTRY;
    }

    /* This is used for as_complete_load */
    struct region *f_region = return_region(as, pt_index); // Get the region to check permissions
    if (f_region == NULL) {
        return EFAULT;
    }

    if (f_region->ker_write) {
        as->page_table[node1][node2] =  as->page_table[node1][node2] | TLBLO_DIRTY;
    }

    return as->page_table[node1][node2];
}

/*
 * Goes through the region data struture and checks if the 
 * vaddr_t exists in any region.
 *
 * Return 0 indicates success
 * 
*/
int validate_region(struct addrspace *as, vaddr_t faultaddress, int faulttype) {
    struct region *curr = as->head;
    int found = 0;

    /* Iterate through the regions and check if vaddr is inbetween any */
    while (curr != NULL) {
        if ((faultaddress >= curr->base_addr) && 
            (faultaddress < curr->base_addr + curr->r_size)) {
                found = 1;
                break;
        }
        curr = curr->next;
    }
    if (!found) {
        return EFAULT;
    }

    if (faulttype == VM_FAULT_READ && !curr->read) {
        return EPERM;       // Cant read from region
    }

    if (faulttype == VM_FAULT_WRITE && !curr->write) {
        return EPERM;       // Cant write to region
    }

    return 0;
}

/*
 * This returns the region a vaddr exists in. 
 * Should not error as it assumes the region exists 
*/
struct region *return_region(struct addrspace *as, vaddr_t faultaddress) {
    struct region *curr = as->head;
    int found = 0;
    while (curr != NULL) {
        if ((faultaddress >= curr->base_addr) && 
            (faultaddress < curr->base_addr + curr->r_size)) {
                found = 1;
                break;
        }
        curr = curr->next;
    }
    if (!found) {
        return NULL;
    }
    return curr;
}


/* Functions to extract first 11 and 
 * 9 bits from VA respectively for pg table
 */

uint32_t get_level_one(vaddr_t vaddrs) {
    uint32_t pt_one = vaddrs & PAGE_FRAME;
    pt_one = pt_one >> 21;
    return pt_one;
}

uint32_t get_level_two(vaddr_t vaddrs) {
    uint32_t pt_two = vaddrs & PAGE_FRAME;
    pt_two = pt_two << 11;
    pt_two = pt_two >> 23;
    return pt_two;
}

/* Load tlb after refill */
void tlb_refill(uint32_t entryhi, uint32_t entrylo) {
    int spl = splhigh();
    tlb_random(entryhi, entrylo);
    splx(spl);
}