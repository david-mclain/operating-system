/*
 * simple9.c
 *
 * Two processes.  2 pages, 2 frames
 * Each writing and reading data from page 0
 * No disk I/O should occur.  0 replaced pages and 2 page faults
 */

#include <usloss.h>
#include <usyscall.h>
#include <phase5.h>
#include <phase3_usermode.h>
#include <string.h>
#include <assert.h>

#define TEST        "simple9"
#define PAGES       2
#define CHILDREN    2
#define FRAMES      2
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
    char   str[64];

    GetPID(&pid);
    USLOSS_Console("\nChild(%d): starting\n", pid);

             //             1         2          3         4
             //   012345678901234567890123456787901234567890
    sprintf(str, "This is the first page for pid %d\n", pid);

    USLOSS_Console("Child(%d): str = %s\n", pid, str);
    USLOSS_Console("Child(%d): strlen(str) = %d\n", pid, strlen(str));

    memcpy(vmRegion, str, strlen(str)+1);  // +1 to copy nul character

    USLOSS_Console("Child(%d): after memcpy\n", pid);

    if (strcmp(vmRegion, str) == 0)
        USLOSS_Console("Child(%d): strcmp first attempt to read worked!\n", pid);
    else
        USLOSS_Console("Child(%d): Wrong string read, first attempt to read\n", pid);

    SemV(sem); // to force context switch

    if (strcmp(vmRegion, str) == 0)
        USLOSS_Console("Child(%d): strcmp second attempt to read worked!\n", pid);
    else
        USLOSS_Console("Child(%d): Wrong string read, second attempt to read\n", pid);

    USLOSS_Console("Child(%d): checking various vmStats\n", pid);

    USLOSS_Console("Child(%d): terminating\n\n", pid);

    Terminate(137);
}



int start5(char *arg)
{
    int  pid[CHILDREN];
    int  status;
    char childName[20], letter;

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
        Spawn(childName, Child, NULL, USLOSS_MIN_STACK*7, PRIORITY, &pid[i]);
    }

    // One P operation per child
    // Both children are low priority, so we want the parent to wait until
    // each child gets past their first strcpy into the VM region.
    for (int i = 0; i < CHILDREN; i++)
        SemP( sem);

    for (int i = 0; i < CHILDREN; i++) {
        Wait(&pid[i], &status);
        assert(status == 137);
    }

    assert(vmStats.faults == 2);
    assert(vmStats.new == 2);
    assert(vmStats.pageOuts == 0);
    assert(vmStats.pageIns == 0);

    USLOSS_Console("start5(): done\n");

    return 0;
}

