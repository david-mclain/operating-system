/**
 * File: phase3.c
 * Authors: David McLain, Miles Gendreau
 *
 * Purpose: Implements Terminal Device driver, with read and
 * write syscalls implemented for the terminal, along with a 
 * syscall to allow processes to sleep for a desired amount
 * of time
 */

#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase3_usermode.h>
#include <phase4.h>
#include <phase4_usermode.h>
#include <usloss.h>

#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#define LEFT(x)     2 * x + 1
#define RIGHT(x)    2 * x + 2
#define PARENT(x)   x / 2

#define SEC_TO_SLEEP_CYCLE(x) x * 10
#define SLEEPING 30

#define MAX_READ_BUFFERS 10
#define RW_MASK_ON  ((0 | 0x4) | 0x2)

#define SECTOR_SIZE 512
#define BLOCKS_PER_TRACK 16
#define TRACK_SIZE SECTOR_SIZE * BLOCKS_PER_TRACK

/* ---------- Data Structures ---------- */

typedef struct PCB {
    int pid;
    int sleepCyclesRemaining;
} PCB;

typedef struct LineBuffer {
    char buffer[MAXLINE];
    int size;
} LineBuffer;

typedef struct DiskState {
    USLOSS_DeviceRequest request;
} DiskState;

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
void kernelDiskSize(USLOSS_Sysargs*);

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

DiskState disks[2];

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

/**
 * Purpose:
 * Initializes data structures used for terminal device drivers,
 * sleep syscall, and syscall function pointers
 *
 * Parameters:
 * None
 *
 * Return:
 * None
 */
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
    systemCallVec[SYS_DISKSIZE] = kernelDiskSize;
}

/**
 * Purpose:
 * Spawns daemon processes used in device drivers
 *
 * Parameters:
 * None
 *
 * Return:
 * None
 */
void phase4_start_service_processes() {
    fork1("Sleep Daemon", sleepDaemonMain, NULL, USLOSS_MIN_STACK, 1);

    // terminal daemons
    for (int i = 0; i < USLOSS_TERM_UNITS; i++) {
        fork1("Term Daemon", termDaemonMain, (char*)(long)i, USLOSS_MIN_STACK, 1);
    }

    // disk daemons
    for (int i = 0; i < USLOSS_DISK_UNITS; i++) {
        fork1("Disk Daemon", diskDaemonMain, (char*)(long)i, USLOSS_MIN_STACK, 1);
    }
}

/* ---------- Syscall Handlers ---------- */

/**
 * Purpose:
 * Function called from syscall vector for sleeping
 *
 * Parameters:
 * USLOSS_Sysargs* args     arguments passed into syscall for how long to sleep
 *
 * Return:
 * None
 */
void kernelSleep(USLOSS_Sysargs* args) {
    args->arg4 = (void*)(long)kernSleep((int)(long)args->arg1);
}

/**
 * Purpose:
 * Handles making process block if it needs to go to sleep and adds it to heap
 *
 * Parameters:
 * int seconds  time we want the process to go to sleep for
 *
 * Return:
 * int  if process went to sleep successfully, 0; else -1
 */
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

void kernelDiskSize(USLOSS_Sysargs* args) {
    int unit = (int)(long)args->arg1;
    if (unit < 0 || unit > 1) {
        args->arg4 = -1;
        return;
    }
    args->arg1 = SECTOR_SIZE;
    args->arg2 = BLOCKS_PER_TRACK;
    FILE* fp = fopen(sprintf("unit%d", unit), "r");
    struct stat st;
    fstat(fileno(fp), &st);
    printf("here3\n");
    args->arg3 = st.st_size / TRACK_SIZE;
}
/**
 * Not implemented in milestone 1
 */
int kernDiskRead(void *diskBuffer, int unit, int track, int first, int sectors, int *status) {}
int kernDiskWrite(void *diskBuffer, int unit, int track, int first, int sectors, int *status) {}
int kernDiskSize(int unit, int *sector, int *track, int *disk) {}

/**
 * Purpose:
 * Function that is called when syscall for term read is called, handles calling proper
 * function to read terminal and setting proper out pointers
 *
 * Parameters:
 * USLOSS_Sysargs* args     arguments to use for term read syscall
 *
 * Return:
 * None
 */
void kernelTermRead(USLOSS_Sysargs* args) {
    int numCharsRead;
    args->arg4 = (void*)(long)kernTermRead(args->arg1, (int)(long)args->arg2, \
            (int)(long)args->arg3, &numCharsRead);
    args->arg2 = (void*)(long)numCharsRead;
}

/**
 * Purpose:
 * Handles reading terminal and getting output from terminal, recieves message
 * from proper mailbox containing string to read
 *
 * Parameters:
 * char* buffer         out pointer to fill up with message from terminal
 * int bufferSize       size of buffer to read from terminal
 * int unitID           terminal device we are reading from
 * int *numCharsRead    out pointer for how many characters are read from terminal
 *
 * Return:
 * int  if terminal read is successful, 0; else -1
 */
int kernTermRead(char *buffer, int bufferSize, int unitID, int *numCharsRead) {
    if (!validateTermArgs(buffer, bufferSize, unitID)) { return -1; }

    MboxSend(termReadRequestMbox[unitID], &bufferSize, sizeof(int));
    *numCharsRead = MboxRecv(termReadMbox[unitID], buffer, bufferSize);
    return 0;
}

/**
 * Purpose:
 * Function that is called when syscall for term write is called, handles calling proper
 * function to write terminal and setting proper out pointers
 *
 * Parameters:
 * USLOSS_Sysargs* args     arguments to use for term write syscall
 *
 * Return:
 * None
 */
void kernelTermWrite(USLOSS_Sysargs* args) {
    int numCharsWritten;
    args->arg4 = (void*)(long)kernTermWrite(args->arg1, (int)(long)args->arg2, \
            (int)(long)args->arg3, &numCharsWritten);
    args->arg2 = (void*)(long)numCharsWritten;
}

/**
 * Purpose:
 * Handles writing to terminal and getting sending input to terminal, sends message
 * to proper mailbox, with string to write to term
 *
 * Parameters:
 * char* buffer         string to write to terminal
 * int bufferSize       size of buffer to write to terminal
 * int unitID           terminal device we are writing to
 * int *numCharsRead    out pointer for how many characters are written to terminal
 *
 * Return:
 * int  if terminal write is successful, 0; else -1
 */
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

/* ---------- Device Drivers ---------- */

int diskDaemonMain(char* args) {
    int status;
    int unit = (int)(long)args;

    while (1) {
        waitDevice(USLOSS_DISK_DEV, unit, &status);
    }

    return 0;
}

/**
 * Purpose:
 * Main function for daemon process that handles any sleeping processes
 *
 * Parameters:
 * char* args   args for function, should be NULL
 *
 * Return:
 * int  if end is ever reached, 0; shouldn't ever return
 */
int sleepDaemonMain(char* args) {
    int status;
    while (1) {
        waitDevice(USLOSS_CLOCK_INT, 0, &status);
        cleanHeap();
    }
    return 0;
}

/**
 * Purpose:
 * Main function for daemon process that handles any terminal interrupts
 *
 * Parameters:
 * char* args   which terminal device the process is handling
 *
 * Return:
 * int  if end is ever reached, 0; shouldn't ever return
 */
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

/**
 * Purpose:
 * Validates input for terminal syscalls
 *
 * Parameters:
 * char* buffer     string for buffer to read/write
 * int bufferSize   size of buffer to read/write
 * int unitID       which terminal device to read/write
 *
 * Return:
 * int  if arguments are valid, 1; else 0
 */
int validateTermArgs(char* buffer, int bufferSize, int unitID) {
    int validBufSize = bufferSize > 0;
    int validUnit = unitID >= 0 && unitID < USLOSS_TERM_UNITS;
    return (validBufSize && validUnit);
}

/**
 * Purpose:
 * Goes through heap of sleeping processes and decrements how many more cycles
 * are remaining for each one sleeping. Then goes through and cleans any process
 * with no time remaining on sleep
 *
 * Parameters:
 * None
 *
 * Return:
 * None
 */
void cleanHeap() {
    for (int i = 0; i < elementsInHeap; i++) {
        sleepHeap[i].sleepCyclesRemaining--;
    }
    while (elementsInHeap > 0 && sleepHeap[0].sleepCyclesRemaining <= 0) {
        heapRemove();
    }
}

/**
 * Purpose:
 * Inserts a new process into the sleep heap
 *
 * Parameters:
 * PCB* proc    process to add to sleep heap
 *
 * Return:
 * None
 */
void insert(PCB* proc) {
    int curIndex = elementsInHeap++;
    PCB* cur = &sleepHeap[curIndex];
    memcpy(cur, proc, sizeof(PCB));
    swim(curIndex);
}

/**
 * Purpose:
 * Removes a process from the root of the heap and wakes it up
 *
 * Parameters:
 * None
 *
 * Return:
 * None
 */
void heapRemove() {
    PCB cur = sleepHeap[0];
    elementsInHeap--;
    memcpy(&sleepHeap[0], &sleepHeap[elementsInHeap], sizeof(PCB));
    sink(0);
    unblockProc(cur.pid);
}

/**
 * Purpose:
 * Performs swim in heap, to have process go to proper location in heap
 *
 * Parameters:
 * int index    index we are swimming from
 *
 * Return:
 * None
 */
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

/**
 * Purpose:
 * Performs sink in heap, to have process go to proper location in heap
 *
 * Parameters:
 * int index    index we are sinking from
 *
 * Return:
 * None
 */
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

/**
 * Purpose:
 * Swaps two processes in heap
 *
 * Parameters:
 * PCB* proc1   first process to swap in heap
 * PCB* proc2   seconds process to swap in heap
 *
 * Return:
 * None
 */
void swap(PCB* proc1, PCB* proc2) {
    PCB temp;
    memcpy(&temp, proc1, sizeof(PCB));
    memcpy(proc1, proc2, sizeof(PCB));
    memcpy(proc2, &temp, sizeof(PCB));
}
