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
#define RW_MASK_ON  ((0 | 0x4) | 0x2)

/* ---------- Data Structures ---------- */

typedef struct PCB {
    int pid;
    int sleepCyclesRemaining;
} PCB;

typedef struct LineBuffer {
    char buffer[MAXLINE];
    int size;
} LineBuffer;

/* ---------- Prototypes ---------- */

void cleanHeap();
void heapRemove();
void insert(PCB*);
void printHeap();
void swap(PCB*, PCB*);
void sink(int);
void swim(int);

void kernelSleep(USLOSS_Sysargs*);
void kernelTermWrite(USLOSS_Sysargs*);
void kernelTermRead(USLOSS_Sysargs*);

int diskDaemonMain(char*);
int sleepDaemonMain(char*);
int termDaemonMain(char*);

int validateTermArgs(char*, int, int);

/* ---------- Globals ---------- */

PCB sleepHeap[MAXPROC];
int elementsInHeap = 0;

int termWriteMutex[USLOSS_TERM_UNITS];
int termWriteMbox[USLOSS_TERM_UNITS];

int termReadRequestMbox[USLOSS_TERM_UNITS];
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
//
// first wakeup problem
// careful with very first read request
// wait for interrupt to occur, daemon handles it
// if device idle then
//     strat 1: special way to wake up daemon without interrupt
//     strat 2: handle something special in syscall itself
//
// have helper function that you call when you want to send a message to disk

void phase4_init(void) {
    memset(sleepHeap, 0, sizeof(sleepHeap));

    // setup ipc stuff for the terminal driver
    for (int i = 0; i < USLOSS_TERM_UNITS; i++) {
        termWriteMutex[i] = MboxCreate(1,0);
        MboxSend(termWriteMutex[i], NULL, 0);
        termWriteMbox[i] = MboxCreate(1, 0);
        termReadRequestMbox[i] = MboxCreate(1, sizeof(int));
        termReadMbox[i] = MboxCreate(1, MAXLINE);
    }

    systemCallVec[SYS_SLEEP] = kernelSleep;
    systemCallVec[SYS_TERMREAD] = kernelTermRead;
    systemCallVec[SYS_TERMWRITE] = kernelTermWrite;
}

void phase4_start_service_processes() {
    fork1("Sleep Daemon", sleepDaemonMain, NULL, USLOSS_MIN_STACK, 1);

    // terminal daemons
    for (int i = 0; i < USLOSS_TERM_UNITS; i++) {
        fork1("Term Daemon", termDaemonMain, (char*)(long)i, USLOSS_MIN_STACK, 1);
    }

    for (int i = 0; i < USLOSS_DISK_UNITS; i++) {
        fork1("Disk Daemon", diskDaemonMain, (char*)(long)i, USLOSS_MIN_STAKC, 1);
    }
}

/* ---------- Syscall Handlers ---------- */

void kernelSleep(USLOSS_Sysargs* args) {
    args->arg4 = (void*)(long)kernSleep((int)(long)args->arg1);
}

int kernSleep(int seconds) {
    if (seconds < 0) { return -1; }
    if (seconds == 0) { return 0; }
    int pid = getpid();
    PCB cur;
    cur.pid = pid;
    cur.sleepCyclesRemaining = SEC_TO_SLEEP_CYCLE(seconds);
    insert(&cur);
    blockMe(SLEEPING);
    return 0;
}

int kernDiskRead(void *diskBuffer, int unit, int track, int first, int sectors, int *status) {}
int kernDiskWrite(void *diskBuffer, int unit, int track, int first, int sectors, int *status) {}
int kernDiskSize(int unit, int *sector, int *track, int *disk) {}

void kernelTermRead(USLOSS_Sysargs* args) {
    int numCharsRead;
    args->arg4 = (void*)(long)kernTermRead(args->arg1, (int)(long)args->arg2, \
            (int)(long)args->arg3, &numCharsRead);
    args->arg2 = (void*)(long)numCharsRead;
}

int kernTermRead(char *buffer, int bufferSize, int unitID, int *numCharsRead) {
    if (!validateTermArgs(buffer, bufferSize, unitID)) { return -1; }

    MboxSend(termReadRequestMbox[unitID], &bufferSize, sizeof(int));
    *numCharsRead = MboxRecv(termReadMbox[unitID], buffer, bufferSize);
    return 0;
}

void kernelTermWrite(USLOSS_Sysargs* args) {
    int numCharsWritten;
    args->arg4 = (void*)(long)kernTermWrite(args->arg1, (int)(long)args->arg2, \
            (int)(long)args->arg3, &numCharsWritten);
    args->arg2 = (void*)(long)numCharsWritten;
}

int kernTermWrite(char *buffer, int bufferSize, int unitID, int *numCharsRead) {
    if (!validateTermArgs(buffer, bufferSize, unitID)) { return -1; }
    MboxRecv(termWriteMutex[unitID], NULL, 0); // gain mutex

    int xmitCtrl; // device control register
    for (int i = 0; i < bufferSize; i++) {
        MboxRecv(termWriteMbox[unitID], NULL, 0);
        xmitCtrl = USLOSS_TERM_CTRL_CHAR(RW_MASK_ON, buffer[i]) | 0x1;
        USLOSS_DeviceOutput(USLOSS_TERM_DEV, unitID, (void*)(long)xmitCtrl);
    }
    *numCharsRead = bufferSize;
                                
    MboxSend(termWriteMutex[unitID], NULL, 0); // release mutex
    return 0;
}

/* ---------- Device Drivers (Daemons) ---------- */

int diskDaemonMain(char* args) {
    int status;
    int unit = (int)(long)args;

    while (1) {
        waitDevice(USLOSS_DISK_DEV, unit, &status);
    }

    return 0;
}

int sleepDaemonMain(char* args) {
    int status;
    while (1) {
        waitDevice(USLOSS_CLOCK_INT, 0, &status);
        cleanHeap();
    }
    return 0;
}

int termDaemonMain(char* args) {
    int id = (int)(long) args;
    USLOSS_DeviceOutput(USLOSS_TERM_DEV, id, (void*)(long)RW_MASK_ON); // unmask interrupts

    int status, statXmit, statRecv; // status stuff

    // read stuff
    int readBuffers = MboxCreate(MAX_READ_BUFFERS, sizeof(LineBuffer)); // stored lines
    int fullBuffers = 0;                // num of lines currenty stored
                                        
    LineBuffer curRecvBuf;              // line currently being read
    char curRecvChar;                   // char currently being read
    curRecvBuf.size = 0;
                                        
    LineBuffer curRequestedBuf;         // used to temp. store a requested line
    int curReqLen;                      // max length requested
    int curSendLen;                     // length of the actually sent line; <= curReqLen
    curRequestedBuf.size = 0;

    while (1) {
        waitDevice(USLOSS_TERM_DEV, id, &status);
        statXmit = USLOSS_TERM_STAT_XMIT(status);
        statRecv = USLOSS_TERM_STAT_RECV(status);

        // handle reading
        if (statRecv == USLOSS_DEV_BUSY) {
            curRecvChar = USLOSS_TERM_STAT_CHAR(status);
            curRecvBuf.buffer[curRecvBuf.size++] = curRecvChar;
            if (curRecvChar == '\n' || curRecvBuf.size >= MAXLINE) {
                if (MboxCondSend(readBuffers, &curRecvBuf, sizeof(curRecvBuf)) >= 0) {
                    fullBuffers++;
                }
                memset(&curRecvBuf, 0, sizeof(curRecvBuf));
            }
        }

        if (fullBuffers > 0 && MboxCondRecv(termReadRequestMbox[id], &curReqLen, sizeof(int)) >= 0) {
            MboxCondRecv(readBuffers, &curRequestedBuf, sizeof(LineBuffer));
            fullBuffers--;
            if (curReqLen < curRequestedBuf.size) { curSendLen = curReqLen; }
            else { curSendLen = curRequestedBuf.size; }
            MboxCondSend(termReadMbox[id], &curRequestedBuf.buffer, curSendLen);
            memset(&curRequestedBuf, 0, sizeof(curRequestedBuf));
        }

        // handle writing
        if (statXmit == USLOSS_DEV_READY) {
            MboxCondSend(termWriteMbox[id], NULL, 0);
        }
    }
    return 0;
}

/* ---------- Helper Functions ---------- */

int validateTermArgs(char* buffer, int bufferSize, int unitID) {
    int validBufSize = bufferSize > 0;
    int validUnit = unitID >= 0 && unitID < USLOSS_TERM_UNITS;
    return (validBufSize && validUnit);
}

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
