/*
 * simple8.c
 *
 * Two children.
 * Each child reads one integer from each page.
 * 4 virtual pages for each process
 * 3 frames
 * Will cause many page faults(!)
 * Nothing gets written to the disk.
 */
#include <usloss.h>
#include <usyscall.h>
#include <phase5.h>
#include <phase3_usermode.h>
#include <string.h>
#include <assert.h>

#define TEST        "simple8"
#define PAGES       4
#define CHILDREN    2
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
        USLOSS_Console("\nChild(%d): iteration %d\n", pid, i);

        // Read one int from the first location on each of the pages
        // in the VM region.
        USLOSS_Console("Child(%d): reading one location from each of %d pages\n",
                 pid, PAGES);
        for (page = 0; page < PAGES; page++) {
            value = * ((int *) (vmRegion + (page * USLOSS_MmuPageSize())));
            USLOSS_Console("Child(%d): page %d, value %d\n", pid, page, value);
            assert(value == 0);
        }

        USLOSS_Console("\nChild(%d): vmStats.faults = %d\n", pid, vmStats.faults);
    }

    USLOSS_Console("\n");
    SemV(sem);
    Terminate(135);
}



int start5(char *arg)
{
    int  pid[CHILDREN];
    int  status;
    char childName[50], letter;

    USLOSS_Console("start5(): Running:    %s\n", TEST);
    USLOSS_Console("start5(): Pagers:     %d\n", PAGERS);
    USLOSS_Console("          Pages:      %d\n", PAGES);
    USLOSS_Console("          Frames:     %d\n", FRAMES);
    USLOSS_Console("          Children:   %d\n", CHILDREN);
    USLOSS_Console("          Iterations: %d\n", ITERATIONS);
    USLOSS_Console("          Priority:   %d\n", PRIORITY);

    assert(vmRegion != NULL);

    SemCreate(0, &sem);

    letter = 'A';
    for (int i = 0; i < CHILDREN; i++) {
        sprintf(childName, "Child%c", letter++);
        Spawn(childName, Child, 0, USLOSS_MIN_STACK*7, PRIORITY, &pid[i]);
    }

    for (int i = 0; i < CHILDREN; i++)
        SemP( sem);

    for (int i = 0; i < CHILDREN; i++) {
        Wait(&pid[i], &status);
        assert(status == 135);
    }

    USLOSS_Console("start5(): done\n");
    //PrintStats();

    return 0;
}

