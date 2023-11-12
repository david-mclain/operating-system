/*
 * simple.c
 *
 * One process, Writing and Reading the same data from the page with
 * a context switch in between.
 * No disk I/O should occur.  0 replaced pages and 1 page 
 * faults.  Check determanistic statistics.
 */

#include <usloss.h>
#include <usyscall.h>
#include <phase5.h>
#include <phase3_usermode.h>
#include <string.h>
#include <assert.h>

#define TEST        "simple1"
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



int Child(char *arg)
{
    int    pid;
    char   str[64]= "This is the first page";

    GetPID(&pid);
    USLOSS_Console("\nChild(%d): starting\n", pid);

    USLOSS_Console("Child(%d): str = %s\n", pid, str);
    USLOSS_Console("Child(%d): strlen(str) = %d\n", pid, strlen(str));

    memcpy(vmRegion, str, strlen(str)+1);  // +1 to copy nul character

    USLOSS_Console("Child(%d): after memcpy\n", pid);

    if (strcmp(vmRegion, str) == 0)
        USLOSS_Console("Child(%d): strcmp first attempt worked!\n", pid);
    else
        USLOSS_Console("Child(%d): Wrong string read, first attempt\n", pid);

    assert(vmStats.faults == 1);
    assert(vmStats.new == 1);

    SemV(sem);  // forces a context switch with start5

    if (strcmp(vmRegion, str) == 0)
        USLOSS_Console("Child(%d): strcmp second attempt worked!\n", pid);
    else
        USLOSS_Console("Child(%d): Wrong string read, second attempt\n", pid);

    USLOSS_Console("Child(%d): checking various vmStats\n", pid);
    assert(vmStats.faults == 1);
    assert(vmStats.new == 1);
    assert(vmStats.pageOuts == 0);
    assert(vmStats.pageIns == 0);

    USLOSS_Console("Child(%d): terminating\n\n", pid);

    Terminate(117);
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
    assert(status == 117);

    USLOSS_Console("start5(): done\n");
    //PrintStats();
    return 0;
}

