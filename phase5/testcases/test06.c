/*
 * simple6.c
 *
 * One process writes every page, where frames = pages-1. If the clock
 * algorithm starts with frame 0, this will cause a page fault on every
 * access.  This test requires writing pages to disk and reading them
 * back in the next time the page is accessed. The test checks for the
 * correct number of page faults, page ins, and page outs.
 */
#include <usloss.h>
#include <usyscall.h>
#include <phase5.h>
#include <phase3_usermode.h>
#include <string.h>
#include <assert.h>

#define TEST        "simple6"
#define PAGES       7
#define CHILDREN    1
#define FRAMES      (PAGES-1)
#define PRIORITY    5
#define ITERATIONS  10
#define PAGERS      1

int vm_num_virtPages    = PAGES;
int vm_num_physPages    = FRAMES;
int vm_num_pagerDaemons = PAGERS;



int sem;



int USLOSS_MmuPageSize(void)
{
    void *pmRegion;
    int   pageSize;
    int   numPages;
    int   numFrames;
    int   mode;

    int rc = USLOSS_MmuGetConfig( &vmRegion, &pmRegion,
                                  &pageSize, &numPages, &numFrames, &mode );
    assert(rc == USLOSS_MMU_OK);
    assert(pageSize == 4096);

    return pageSize;
}



int Child(char *arg)
{
    int      pid;
    int      page;
    int      i;
    //char     *buffer;
    VmStats  before;
    int      value;

    GetPID(&pid);
    USLOSS_Console("\nChild(%d): starting\n", pid);

    //buffer = (char *) vmRegion;

    for (i = 0; i < ITERATIONS; i++) {
        USLOSS_Console("\nChild(%d): iteration %d\n", pid, i);
        before = vmStats;
        for (page = 0; page < PAGES; page++) {
            USLOSS_Console("Child(%d): writing to page %d\n", pid, page);
            // Write page as an int to the 1st 4 bytes of the page
            * ((int *) (vmRegion + (page * USLOSS_MmuPageSize()))) = page;
            // Write page as a letter to the 5th byte of the page
            * ((char *) (vmRegion + (page * USLOSS_MmuPageSize()) + 4)) =
                'A' + page;
            value = * ((int *) (vmRegion + (page * USLOSS_MmuPageSize())));
            assert(value == page);
        }
        assert(vmStats.faults - before.faults == PAGES);
    }
    assert(vmStats.faults == PAGES * ITERATIONS);
    assert(vmStats.new == PAGES);
    assert(vmStats.pageIns == PAGES * (ITERATIONS - 1));
    assert(vmStats.pageOuts == PAGES * ITERATIONS - FRAMES);

    SemV(sem);

    USLOSS_Console("\n");

    Terminate(127);
}



int start5(char *arg)
{
    int  pid;
    int  status;

    USLOSS_Console("start5(): Running:    %s\n", TEST);
    USLOSS_Console("start5(): Pagers:     %d\n", PAGERS);
    USLOSS_Console("          Pages:      %d\n", PAGES);
    USLOSS_Console("          Frames:     %d\n", FRAMES);
    USLOSS_Console("          Children:   %d\n", CHILDREN);
    USLOSS_Console("          Iterations: %d\n", ITERATIONS);
    USLOSS_Console("          Priority:   %d\n", PRIORITY);

    Spawn("Child", Child,  0,USLOSS_MIN_STACK*7,PRIORITY, &pid);
    SemP( sem);
    Wait(&pid, &status);
    assert(status == 127);

    USLOSS_Console("start5(): done\n");
    //PrintStats();

    return 0;
}

