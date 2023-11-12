#include <phase5.h>

typedef struct VirtPageMetaData {

} VirtPageMetaData;

/* the phase 5 code *MUST* supply this variable!  The testcases will
 * rely on it
 */
void* vmRegion;

VirtPageMetaData vp_metadata[VM_MAX_VIRT_PAGES];

VmStats	vmStats;
void PrintStats();

void phase5_init(void) {
    //USLOSS_MmuInit(numMaps, vm_num_pages, , USLOSS_MMU_MODE_PAGETABLE)
}

void phase5_start_service_processes() {

}


USLOSS_PTE* phase5_mmu_pageTable_alloc(int pid) {
    return NULL;
}

void phase5_mmu_pageTable_free(int pid, USLOSS_PTE* pte) {

}
