/*
 * simple2.c
 * Reads bytes from page 0 of the vmRegion to be sure they are
 * all set to 0.
 *
 */
#include <usloss.h>
#include <usyscall.h>
#include <phase5.h>
#include <phase3_usermode.h>
#include <string.h>
#include <assert.h>

#define TEST        "simple2"
#define PAGES       1
#define CHILDREN    1
#define FRAMES      1
#define PRIORITY    5
#define ITERATIONS  1
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
    int   pid;
    int   i, memOkay;
    char *buffer;

    GetPID(&pid);
    USLOSS_Console("\nChild(%d): starting\n", pid);

    memOkay = 1;
    buffer = (char *) vmRegion;
    for ( i = 0; i < USLOSS_MmuPageSize(); i++ )
        if ( buffer[i] != 0 )
            memOkay = 0;

    if ( memOkay )
        USLOSS_Console("Child(%d): vmRegion is filled with 0's\n", pid);
    else
        USLOSS_Console("Child(%d): vmRegion is NOT zero-filled!\n", pid);

    assert(vmStats.faults == 1);

    SemV(sem);

    USLOSS_Console("Child(%d): terminating\n\n", pid);

    Terminate(119);
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

    Spawn("Child", Child, NULL, USLOSS_MIN_STACK * 7, PRIORITY, &pid);

    SemP( sem);

    Wait(&pid, &status);
    assert(status == 119);

    USLOSS_Console("start5 done\n");
    //PrintStats();
    return 0;
}

