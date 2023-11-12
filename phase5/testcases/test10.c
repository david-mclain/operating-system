/*
 * simple10.c
 *
 * Two processes. Two pages of virtual memory. Two frames.
 * Each writing and reading data from both page 0 and page 1 with
 *    a context switch in between.
 * Should cause page 0 of each process to be written to disk
 * ChildA writes to page 0, in frame 0
 * ChildB writes to page 0, in frame 1
 * ChildA writes to page 1, in frame 0, sends A's page 0 to disk
 * ChildB writes to page 1, in frame 1, sends B's page 0 to disk
 */

#include <usloss.h>
#include <usyscall.h>
#include <phase5.h>
#include <phase3_usermode.h>
#include <string.h>
#include <assert.h>

#define TEST        "simple10"
#define PAGES       2
#define CHILDREN    2
#define FRAMES      2
#define PRIORITY    5
#define ITERATIONS  2
#define PAGERS      1

int vm_num_virtPages    = PAGES;
int vm_num_physPages    = FRAMES;
int vm_num_pagerDaemons = PAGERS;



extern void printPageTable(int pid);
extern void printFrameTable();

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
    int    pid;
    char   str[64];
    char   toPrint[64];
    

    GetPID(&pid);
    USLOSS_Console("\nChild(%d): starting\n", pid);
    
             //             1         2          3         4
             //   012345678901234567890123456787901234567890
    sprintf(str, "This is the first page, pid = %d", pid);

    for (int i = 0; i < ITERATIONS; i++) {

        switch (i) {
        case 0:
            sprintf(toPrint, "%c: This is page zero, pid = %d", *arg, pid);
            break;
        case 1:
            sprintf(toPrint, "%c: This is page one, pid = %d", *arg, pid);
            break;
        }
        USLOSS_Console("Child(%d): toPrint = '%s'\n", pid, toPrint);
        USLOSS_Console("Child(%d): strlen(toPrint) = %d\n", pid, strlen(toPrint));

        // memcpy causes a page fault
        memcpy(vmRegion + i*USLOSS_MmuPageSize(), toPrint,
               strlen(toPrint)+1);  // +1 to copy nul character

        USLOSS_Console("Child(%d): after memcpy on iteration %d\n", pid, i);

        if (strcmp(vmRegion + i*USLOSS_MmuPageSize(), toPrint) == 0)
            USLOSS_Console("Child(%d): strcmp first attempt to read worked!\n", pid);
        else {
            USLOSS_Console("Child(%d): Wrong string read, first attempt to read\n",
                     pid);
            USLOSS_Console("  read: '%s'\n", vmRegion + i*USLOSS_MmuPageSize());
            USLOSS_Halt(1);
        }

        SemV(sem);  // to force a context switch

        if (strcmp(vmRegion + i*USLOSS_MmuPageSize(), toPrint) == 0)
            USLOSS_Console("Child(%d): strcmp second attempt to read worked!\n", pid);
        else {
            USLOSS_Console("Child(%d): Wrong string read, second attempt to read\n",
                     pid);
            USLOSS_Console("  read: '%s'\n", vmRegion + i*USLOSS_MmuPageSize());
            USLOSS_Halt(1);
        }

    } // end loop for i

    USLOSS_Console("Child(%d): checking various vmStats\n", pid);
    USLOSS_Console("Child(%d): terminating\n\n", pid);
    Terminate(137);
}



int start5(char *arg)
{
    int  pid[CHILDREN];
    int  status;
    char toPass;
    char buffer[20];

    USLOSS_Console("start5(): Running:    %s\n", TEST);
    USLOSS_Console("start5(): Pagers:     %d\n", PAGERS);
    USLOSS_Console("          Pages:      %d\n", PAGES);
    USLOSS_Console("          Frames:     %d\n", FRAMES);
    USLOSS_Console("          Children:   %d\n", CHILDREN);
    USLOSS_Console("          Iterations: %d\n", ITERATIONS);
    USLOSS_Console("          Priority:   %d\n", PRIORITY);

    assert(vmRegion != NULL);

    SemCreate(0, &sem);

    toPass = 'A';
    for (int i = 0; i < CHILDREN; i++) {
        sprintf(buffer, "Child%c", toPass);
        Spawn(buffer, Child, &toPass, USLOSS_MIN_STACK * 7, PRIORITY, &pid[i]);
        toPass = toPass + 1;
    }

    // One P operation per (CHILDREN * ITERATIONS)
    for (int i = 0; i < ITERATIONS*CHILDREN; i++)
        SemP( sem);

    for (int i = 0; i < CHILDREN; i++) {
        Wait(&pid[i], &status);
        assert(status == 137);
    }

    //PrintStats();
    assert(vmStats.faults == 4);
    assert(vmStats.new == 4);
    assert(vmStats.pageOuts == 2);
    assert(vmStats.pageIns == 0);

    USLOSS_Console("start5(): done\n");
    //PrintStats();

    return 0;
}

