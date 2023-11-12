/*
 * Definitions for Phase 5 of the project (virtual memory).
 */
#ifndef _PHASE5_H
#define _PHASE5_H

#include <usloss.h>
#include <stdio.h>
#include <stdlib.h>
#include <phase1.h>
#include <phase2.h>



/*
 * Pager priority.
 */
#define PAGER_PRIORITY	2

/*
 * Maximum number of pagers.
 */
#define MAXPAGERS 4

/*
 * Largest # of virtual memory pages in the vmRegion
 */
#define VM_MAX_VIRT_PAGES 64

/*
 * Largest # of physical memory pages
 */
#define VM_MAX_PHYS_PAGES 256

/*
 * Largest swap disk size
 */
#define VM_MAX_SWAP_PAGES 1024



/* the testcase *MUST* supply these variables!  The Phase 5 code will
 * rely on them.
 */
extern int vm_num_virtPages;
extern int vm_num_physPages;
extern int vm_num_pagerDaemons;

/* the phase 5 code *MUST* supply this variable!  The testcases will
 * rely on it
 */
extern void *vmRegion;



/*
 * Paging statistics
 */
typedef struct VmStats {
    int pages;          // Size of VM region, in pages
    int frames;         // Size of physical memory, in frames
    int diskBlocks;     // Size of disk, in blocks (pages)
    int freeFrames;     // # of frames that are not in-use
    int freeDiskBlocks; // # of blocks that are not in-use
    int switches;       // # of context switches
    int faults;         // # of page faults
    int new;            // # faults caused by previously unused pages
    int pageIns;        // # faults that required reading page from disk
    int pageOuts;       // # faults that required writing a page to disk
    int replaced;	// # pages replaced; i.e., frame had a page and we
                        //   replaced that page in the frame with a different
                        //   page. */
} VmStats;

extern VmStats	vmStats;
extern void PrintStats();

extern void phase5_init(void);



/* the direct-from-kernel versions of a couple Phase 4 functions.  These have
 * the exact same semantics as their Phase 4 versions, except that they must
 * be called from the kernel.
 */
extern  int  kernDiskRead (void *diskBuffer, int unit, int track, int first, 
                           int sectors, int *status);
extern  int  kernDiskWrite(void *diskBuffer, int unit, int track, int first,
                           int sectors, int *status);
extern  int  kernDiskSize (int unit, int *sector, int *track, int *disk);


#endif /* _PHASE5_H */
