/*
 * simple3.c
 * Writes bytes into all 3 pages of the vmRegion.
 * Should see 3 page faults.
 *
 */
#include <usloss.h>
#include <usyscall.h>
#include <phase5.h>
#include <phase3_usermode.h>
#include <string.h>
#include <assert.h>

#define TEST        "simple3"
#define PAGES       3
#define CHILDREN    1
#define FRAMES      3
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
    int  pid;
    int  i;
    char *buffer;

    GetPID(&pid);
    USLOSS_Console("\nChild(%d): starting\n", pid);

    buffer = (char *) vmRegion;
    for (i = 0; i < PAGES * USLOSS_MmuPageSize(); i++) {
        buffer[i] = i % 256;
    }

    assert(vmStats.faults == PAGES);

    SemV(sem);

    USLOSS_Console("Child(%d): terminating\n\n", pid);
    Terminate(121);
}



int start5(char *arg)
{
    int pid;
    int status;

    USLOSS_Console("start5(): Running:    %s\n", TEST);
    USLOSS_Console("start5(): Pagers:     %d\n", PAGERS);
    USLOSS_Console("          Pages:      %d\n", PAGES);
    USLOSS_Console("          Frames:     %d\n", FRAMES);
    USLOSS_Console("          Children:   %d\n", CHILDREN);
    USLOSS_Console("          Iterations: %d\n", ITERATIONS);
    USLOSS_Console("          Priority:   %d\n", PRIORITY);

    assert(vmRegion != NULL);

    status = SemCreate(0, &sem);
    assert(status == 0);

    Spawn("Child", Child,  0, USLOSS_MIN_STACK*7, PRIORITY, &pid);

    SemP( sem);
    Wait(&pid, &status);
    assert(status == 121);

    USLOSS_Console("start5(): done\n");
    //PrintStats();

    return 0;
}

