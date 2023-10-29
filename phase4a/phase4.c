#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase3_usermode.h>
#include <phase4.h>
#include <phase4_usermode.h>
#include <usloss.h>

#include <string.h>
#include <stdio.h>

#define LEFT(x)     2 * x + 1
#define RIGHT(x)    2 * x + 2
#define PARENT(x)   x / 2

#define SEC_TO_SLEEP_CYCLE(x) x * 10
#define SLEEPING 30

#define MAX_READ_BUFFERS 10

/* ---------- Data Structures ---------- */

typedef struct PCB {
    int pid;
    int sleepCyclesRemaining;
    // idk something something blah blah
} PCB;

/* ---------- Prototypes ---------- */

void swap(PCB*, PCB*);
void sink(int);
void swim(int);
void cleanHeap();
void heapRemove();
void insert(PCB*);

void printHeap();
void kernelSleep(USLOSS_Sysargs*);

int daemonMain(char*);

/* ---------- Globals ---------- */

PCB processes[MAXPROC];
PCB sleepHeap[MAXPROC];
int elementsInHeap = 0;
int sleepDaemon;

int termWriteMutex[USLOSS_TERM_UNITS];
int termWriteMbox[USLOSS_TERM_UNITS];
int termReadMbox[USLOSS_TERM_UNITS];

/* ---------- Phase 4 Functions ---------- */

// implement elevator algorithm for disk
//
// create global clock based on ticks remaining for wait device
// use daemon for sleep stuff blah blah
//
// use dma to communicate with disk
// make req struct in memory, includes pointer to buffer + fields for op
//
// cpu writes to device a pointer to request struct we want device to run
// have daemon have queue of requests to send to disk

void phase4_init(void) {
    memset(sleepHeap, 0, sizeof(sleepHeap));
    memset(processes, 0, sizeof(processes));

    // setup ipc stuff for the terminal driver
    int termCtrl = 0;
    for (int i = 0; i < USLOSS_TERM_UNITS; i++) {
        termWriteMutex[i] = MboxCreate(1,0);
        MboxSend(termWriteMutex[i], NULL, 0);
        termWriteMbox[i] = MboxCreate(1,sizeof(char));
        termReadMbox[i] = MboxCreate(1, MAXLINE); // +1 for null term? or don't need to do that?

        // unmask interrupts for reading & writing
        USLOSS_TERM_CTRL_XMIT_INT(termCtrl);
        USLOSS_TERM_CTRL_RECV_INT(termCtrl);
        USLOSS_DeviceOutput(USLOSS_TERM_DEV, i, &termCtrl);
    }

    systemCallVec[SYS_SLEEP] = kernelSleep;
}

void phase4_start_service_processes() {
    sleepDaemon = fork1("Sleep Daemon", daemonMain, NULL, USLOSS_MIN_STACK, 1);
    PCB* cur = &processes[sleepDaemon % MAXPROC];
    cur->pid = sleepDaemon;
}

/* ---------- Syscall Handlers ---------- */

void kernelSleep(USLOSS_Sysargs* args) {
    args->arg4 = (void*)(long)kernSleep((int)(long)args->arg1);
}

int kernSleep(int seconds) {
    if (seconds < 0) { return -1; }
    if (seconds == 0) { return 0; }
    int pid = getpid();
    PCB* cur = &processes[pid % MAXPROC];
    cur->pid = pid;
    cur->sleepCyclesRemaining = SEC_TO_SLEEP_CYCLE(seconds);
    insert(cur);
    blockMe(SLEEPING);
    return 0;
}

int kernDiskRead(void *diskBuffer, int unit, int track, int first, int sectors, int *status) {

}

int kernDiskWrite(void *diskBuffer, int unit, int track, int first, int sectors, int *status) {
}

int kernDiskSize(int unit, int *sector, int *track, int *disk) {

}

int kernTermRead(char *buffer, int bufferSize, int unitID, int *numCharsRead) {

}

int kernTermWrite(char *buffer, int bufferSize, int unitID, int *numCharsRead) {
    // check input
    
    MboxRecv(termWriteMutex[unitID], NULL, 0); // gain mutex
    
    // send characters one-by-one to the driver using a mailbox
    /*
    for (int i = 0; i < bufferSize; i++) {
        MboxSend(
    }
    */

    MboxSend(termWriteMutex[unitID], NULL, 0); // release mutex
    return 0;
}

/* ---------- Device Drivers ---------- */

int daemonMain(char* args) {
    int status;
    while (1) {
        waitDevice(USLOSS_CLOCK_INT, 0, &status);
        cleanHeap();
    }
    return 0; // shouldn't get here?
}

int termDaemonMain(char* args) {
    int id = (int)(long) args;
    int termReadBuffers = MboxCreate(MAX_READ_BUFFERS, MAXLINE);

    int status, statXmit, statRecv;
    while (1) {
        waitDevice(USLOSS_TERM_DEV, id, &status);
        statXmit = USLOSS_TERM_STAT_XMIT(status);
        statRecv = USLOSS_TERM_STAT_RECV(status);

        if (statXmit == USLOSS_DEV_READY) {
            // ready to send next character
        }

        if (statRecv == USLOSS_DEV_BUSY) {
            // just recieved a character, do something w it
        }
    }

    return 0;
}

/* ---------- Helper Functions ---------- */

// decrement all sleepCyclesRemaining in heap, remove if needed
// while sleepHeap[0].cycles = 0 remove and unblock
void cleanHeap() {
    for (int i = 0; i < elementsInHeap; i++) {
        sleepHeap[i].sleepCyclesRemaining--;
    }
    while (elementsInHeap > 0 && sleepHeap[0].sleepCyclesRemaining <= 0) {
        heapRemove();
    }
}

void insert(PCB* proc) {
    int curIndex = elementsInHeap++;
    PCB* cur = &sleepHeap[curIndex];
    memcpy(cur, proc, sizeof(PCB));
    swim(curIndex);
}

void heapRemove() {
    PCB cur = sleepHeap[0];
    elementsInHeap--;
    memcpy(&sleepHeap[0], &sleepHeap[elementsInHeap], sizeof(PCB));
    sink(0);
    unblockProc(cur.pid);
}

void printHeap() {
    for (int i = 0; i < elementsInHeap; i++) {
        printf("proc: %d; cycles remaining: %d\n", sleepHeap[i].pid, sleepHeap[i].sleepCyclesRemaining);
    }
}

void swim(int index) {
    int i = index, j = PARENT(index);
    while (j >= 0) {
        if (sleepHeap[i].sleepCyclesRemaining < sleepHeap[j].sleepCyclesRemaining) {
            swap(&sleepHeap[i], &sleepHeap[j]);
            i = j;
            j = PARENT(j);
        }
        else { break; }
    }
}

void sink(int index) {
    int small = index;
    int left = LEFT(index);
    int right = RIGHT(index);
    if (left < elementsInHeap && sleepHeap[left].sleepCyclesRemaining < sleepHeap[small].sleepCyclesRemaining) {
        small = left;
    }
    if (right < elementsInHeap && sleepHeap[right].sleepCyclesRemaining < sleepHeap[small].sleepCyclesRemaining) {
        small = right;
    }
    if (small != index) {
        swap(&sleepHeap[index], &sleepHeap[small]);
        sink(small);
    }
}

void swap(PCB* proc1, PCB* proc2) {
    PCB temp;
    memcpy(&temp, proc1, sizeof(PCB));
    memcpy(proc1, proc2, sizeof(PCB));
    memcpy(proc2, &temp, sizeof(PCB));
}

