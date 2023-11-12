/*
 * simple4.c
 *
 * One process reads every byte of every page, where frames = pages-1. If the
 * clock algorithm starts with frame 0, this will cause a page fault on every
 * access. 
 */
#include <usloss.h>
#include <usyscall.h>
#include <phase5.h>
#include <phase3_usermode.h>
#include <string.h>
#include <assert.h>

#define TEST        "simple4"
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
    int     pid;
    int     page;
    int     i;
    int     value;

    GetPID(&pid);
    USLOSS_Console("\nChild(%d): starting\n", pid);


    for (i = 0; i < ITERATIONS; i++) {
        VmStats before = vmStats;

        USLOSS_Console("\nChild(%d): iteration %d\n", pid, i);
        USLOSS_Console("before.faults = %d\n", before.faults);

        // Read one int from the first location on each of the pages
        // in the VM region.
        USLOSS_Console("Child(%d): reading one location from each of %d pages\n", pid, PAGES);
        for (page = 0; page < PAGES; page++) {
            value = * ((int *) (vmRegion + (page * USLOSS_MmuPageSize())));
            USLOSS_Console("Child(%d): page %d, value %d\n", pid, page, value);
            assert(value == 0);
        }

        USLOSS_Console("Child(%d): vmStats.faults = %d\n", pid, vmStats.faults);
        // The number of faults should equal the number of pages
        assert(vmStats.faults - before.faults == PAGES);
    }

    USLOSS_Console("\n");
    SemV(sem);
    Terminate(123);
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

    assert(vmRegion != NULL);

    SemCreate(0, &sem);

    Spawn("Child", Child,  0,USLOSS_MIN_STACK*7,PRIORITY, &pid);

    SemP( sem);
    Wait(&pid, &status);
    assert(status == 123);

    USLOSS_Console("start5(): done\n");
    //PrintStats();

    return 0;
}

