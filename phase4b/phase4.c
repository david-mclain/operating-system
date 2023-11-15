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
#define AWAITING_DISK 40

#define print USLOSS_Console

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
    int currentTrack;
    int status;
    int tracks;

    USLOSS_DeviceRequest request;
} DiskState;

typedef struct DiskRequest {
    int currentTask;
    int currentTrack;
    int currentSector;
    int process;
    int requests;
    int requestsFulfilled;
    int sectorOffset;
    
    struct DiskRequest* nextRequest;
    struct DiskRequest* prevRequest;

    void* buffer;
} DiskRequest;

/* ---------- Prototypes ---------- */

int calculateRequests(int, int, int, int);
int validateTermArgs(char*, int, int);

long calculateLocation(int, int);

void addToRequestQueue(DiskRequest*, int);
void fillRequest(DiskRequest*, int, int, int, int, int);
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
void kernelDiskRead(USLOSS_Sysargs*);
void kernelDiskWrite(USLOSS_Sysargs*);

/* ---------- Daemon Prototypes ---------- */

int diskDaemonMain(char*);
int sleepDaemonMain(char*);
int termDaemonMain(char*);
int tempDaemon(char*);

// your mom lol <3 miles dont delete this bestie thx

/* ---------- Globals ---------- */

PCB sleepHeap[MAXPROC];
int elementsInHeap = 0;

int termWriteMutex[USLOSS_TERM_UNITS];
int termWriteMbox[USLOSS_TERM_UNITS];

int termReadRequestMbox[USLOSS_TERM_UNITS];
int termReadMbox[USLOSS_TERM_UNITS];

int diskMbox[USLOSS_DISK_UNITS];

DiskState disks[USLOSS_DISK_UNITS];
DiskRequest diskRequests[USLOSS_DISK_UNITS][MAXPROC];

DiskRequest* diskQueues[USLOSS_DISK_UNITS];
DiskRequest* curReqs[USLOSS_DISK_UNITS];
// just use mailboxes lol
// kate was here :)

/* ---------- Phase 4 Functions ---------- */

// what if no daemon and just waitdevice lol
// maybe work
// daemon only maintain queue
// processes wait for dev on own
// wait no just use daemon to keep track of who goes next
// then have each call waitDevice on its own

// implement elevator algorithm for disk
//
// create global clock based on ticks remaining for wait device
// use daemon for sleep stuff blah blah
//
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
    memset(disks, 0, sizeof(disks));
    memset(diskQueues, 0, sizeof(diskQueues));

    // setup ipc stuff for the terminal driver
    for (int i = 0; i < USLOSS_TERM_UNITS; i++) {
        termWriteMutex[i] = MboxCreate(1,0);
        MboxSend(termWriteMutex[i], NULL, 0);
        termWriteMbox[i] = MboxCreate(1, 0);
        termReadRequestMbox[i] = MboxCreate(1, sizeof(int));
        termReadMbox[i] = MboxCreate(1, MAXLINE);
    }

    for (int i = 0; i < USLOSS_DISK_UNITS; i++) {
        diskMbox[i] = MboxCreate(0, 0);
    }

    systemCallVec[SYS_SLEEP] = kernelSleep;
    systemCallVec[SYS_TERMREAD] = kernelTermRead;
    systemCallVec[SYS_TERMWRITE] = kernelTermWrite;
    systemCallVec[SYS_DISKSIZE] = kernelDiskSize;
    systemCallVec[SYS_DISKREAD] = kernelDiskRead;
    systemCallVec[SYS_DISKWRITE] = kernelDiskWrite;
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
    // sleep daemon
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

void removeFromRequestQueue(DiskRequest* curRequest, int unit) {
    if (curRequest->nextRequest) { 
        curRequest->nextRequest->prevRequest = curRequest->prevRequest;
    }
    if (curRequest->prevRequest) { 
        curRequest->prevRequest->nextRequest = curRequest->nextRequest;
    }
    if (curRequest == diskQueues[unit]) { 
        diskQueues[unit] = curRequest->nextRequest;
    }
}

void kernelDiskSize(USLOSS_Sysargs* args) {
    int unit = (int)(long)args->arg1;
    if (unit < 0 || unit > 1) {
        args->arg4 = -1;
        return;
    }
    args->arg4 = 0;

    DiskState* disk = &disks[unit];
    if (!disk->tracks) {
        int pid = getpid();
        DiskRequest* curRequest = &diskRequests[pid % MAXPROC];
        fillRequest(curRequest, USLOSS_DISK_TRACKS, 0, 0, pid, 0);
        addToRequestQueue(curRequest, unit);
        if (curReqs[unit]->process != pid) {
            blockMe(AWAITING_DISK);
        }

        // THE COOKING BEGINS
        disk->request.opr = USLOSS_DISK_TRACKS;
        disk->request.reg1 = (void*)&disk->tracks;
        USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &(disk->request));
        blockMe(AWAITING_DISK);
        removeFromRequestQueue(curRequest, unit);
        if (!curReqs[unit]->nextRequest && diskQueues[unit] && diskQueues[unit] != curReqs[unit]) {
            curReqs[unit] = diskQueues[unit];
        }
        else { curReqs[unit] = curReqs[unit]->nextRequest; }

        if (curReqs[unit]) { unblockProc(curReqs[unit]->process); }
    }
    args->arg1 = SECTOR_SIZE;
    args->arg2 = BLOCKS_PER_TRACK;
    args->arg3 = disk->tracks;
    //USLOSS_Console("finished size\n");
}

void kernelDiskRead(USLOSS_Sysargs* args) {
    void* buffer = args->arg1;
    int sectors = (int)(long)args->arg2;
    int track = (int)(long)args->arg3;
    int block = (int)(long)args->arg4;
    int unit = (int)(long)args->arg5;
    int pid = getpid();
    if (!disks[unit].tracks) {
        USLOSS_Sysargs temp;
        temp.arg1 = (void*)(long)unit;
        kernelDiskSize(&temp);
    }

    if (unit < 0 || unit > 1 || track < 0 || block < 0 || block >= BLOCKS_PER_TRACK) {
        args->arg4 = -1;
        return;
    }
    args->arg4 = 0;
    // just block and unblock manually and have processes keep track of what disk task they need to do
    //
    // have queue keeping track of which process to move to next
    DiskRequest* curRequest = &diskRequests[unit][pid % MAXPROC];
    fillRequest(curRequest, USLOSS_DISK_READ, track, block, pid, sectors);
    addToRequestQueue(curRequest, unit);
    
    if (curReqs[unit]->process != pid) { blockMe(AWAITING_DISK); }

    DiskState* disk = &disks[unit];
    while (curReqs[unit]->requestsFulfilled < curReqs[unit]->requests) {
        if (disk->currentTrack != curReqs[unit]->currentTrack) {
            disk->request.opr = USLOSS_DISK_SEEK;
            disk->request.reg1 = curReqs[unit]->currentTrack;
            disk->currentTrack = curReqs[unit]->currentTrack;
        }
        else {
            disk->request.opr = USLOSS_DISK_READ;
            disk->request.reg1 = (void*)(long)curReqs[unit]->currentSector;
            disk->request.reg2 = buffer + curReqs[unit]->sectorOffset++ * SECTOR_SIZE;
            curReqs[unit]->currentSector = (curReqs[unit]->currentSector + 1) % BLOCKS_PER_TRACK;
            curReqs[unit]->requestsFulfilled++;
        }
        USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &(disk->request));
        blockMe(AWAITING_DISK);
        if (disk->status == USLOSS_DEV_ERROR) { break; }
    }

    removeFromRequestQueue(curRequest, unit);
    if (!curReqs[unit]->nextRequest && diskQueues[unit] && diskQueues[unit] != curReqs[unit]) { 
        curReqs[unit] = diskQueues[unit];
    }
    else { curReqs[unit] = curReqs[unit]->nextRequest; }

    if (curReqs[unit]) { unblockProc(curReqs[unit]->process); }
    //USLOSS_Console("finished read\n");
    args->arg1 = disk->status == USLOSS_DEV_ERROR ? disk->status : 0;
}

void kernelDiskWrite(USLOSS_Sysargs* args) {
    void* buffer = args->arg1;
    int sectors = (int)(long)args->arg2;
    int track = (int)(long)args->arg3;
    int block = (int)(long)args->arg4;
    int unit = (int)(long)args->arg5;
    if (!disks[unit].tracks) {
        USLOSS_Sysargs temp;
        temp.arg1 = unit;
        kernelDiskSize(&temp);
    }

    if (unit < 0 || unit > 1 || track < 0 || block < 0 || block >= BLOCKS_PER_TRACK) {
        args->arg4 = -1;
        return;
    }
    args->arg4 = 0;

    int pid = getpid();
    DiskRequest* curRequest = &diskRequests[unit][pid % MAXPROC];
    fillRequest(curRequest, USLOSS_DISK_WRITE, track, block, pid, sectors);
    addToRequestQueue(curRequest, unit);
    curRequest->buffer = buffer;

    DiskRequest* queue = diskQueues[unit];
    // change to curRequest
    if (diskQueues[unit] != curRequest) { blockMe(AWAITING_DISK); }

    DiskState* disk = &disks[unit];
    while (curReqs[unit]->requestsFulfilled < curReqs[unit]->requests) {
        if (disk->currentTrack != curReqs[unit]->currentTrack) {
            disk->request.opr = USLOSS_DISK_SEEK;
            disk->request.reg1 = curReqs[unit]->currentTrack;
            disk->currentTrack = curReqs[unit]->currentTrack;
        }
        else {
            disk->request.opr = USLOSS_DISK_WRITE;
            disk->request.reg1 = (void*)(long)curReqs[unit]->currentSector;
            disk->request.reg2 = buffer + curReqs[unit]->sectorOffset++ * SECTOR_SIZE;
            curReqs[unit]->currentSector = (curReqs[unit]->currentSector + 1) % BLOCKS_PER_TRACK;
            curReqs[unit]->requestsFulfilled++;
        }
        USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &(disk->request));
        blockMe(AWAITING_DISK);
        if (disk->status == USLOSS_DEV_ERROR) { break; }
    }
    removeFromRequestQueue(curRequest, unit);
    if (!curReqs[unit]->nextRequest && diskQueues[unit] && diskQueues[unit] != curReqs[unit]) {
        curReqs[unit] = diskQueues[unit];
    }
    else { curReqs[unit] = curReqs[unit]->nextRequest; }

    if (curReqs[unit]) { unblockProc(curReqs[unit]->process); }
    // block process since in queue
    // loop through every request it needs to complete after since itll be woken up when turn
    // sleep everytime it needs to make new request and daemon handles waking up
    //USLOSS_Console("finished write\n");
    args->arg1 = disk->status == USLOSS_DEV_ERROR ? disk->status : 0;
}

void fillRequest(DiskRequest* curRequest, int task, int track, int block, int pid, int sectors) {
    memset(curRequest, 0, sizeof(DiskRequest));
    curRequest->currentTask = task;
    curRequest->process = pid;
    curRequest->requests = sectors;
    if (track) {
        curRequest->currentTrack = track;
        curRequest->currentSector = block;
        curRequest->sectorOffset = 0;
    }
}

int calculateRequests(int task, int track, int block, int sectors) {
    return track ? 1 + sectors + ((block + sectors) / BLOCKS_PER_TRACK) : 1;
}

void addToRequestQueue(DiskRequest* curRequest, int unit) {
    //DiskRequest* queue = diskQueues[unit];
    if (!diskQueues[unit]) {
        diskQueues[unit] = curRequest;
        curReqs[unit] = curRequest;
    }
    else {
        DiskRequest* cur = diskQueues[unit];
        while (cur->nextRequest && cur->currentTrack < curRequest->currentTrack) {
            cur = cur->nextRequest;
        }
        cur->nextRequest = curRequest;
        // its onyl track based thats actually so much easier
    }
    //printQueues();
}

void printQueues() {
    for (int i = 0; i < USLOSS_DISK_UNITS; i++) {
        DiskRequest* cur = diskQueues[i];
        printf("\n");
        printf("disk %d\n", i);
        printf("cur = %p\n", cur);
        while (cur) {
            printf("current task:             %d\n", cur->currentTask);
            printf("current track:            %d\n", cur->currentTrack);
            printf("current sector:           %d\n", cur->currentSector);
            printf("current process:          %d\n", cur->process);
            printf("current requestRemaining: %d\n", cur->requests);
            printf("current sectorOffset:     %d\n", cur->sectorOffset);
            printf("->");
            cur = cur->nextRequest;
        }
        printf("\n");
    }
}
/*
typedef struct DiskRequest {
    int currentTask;
    int currentTrack;
    int currentSector;
    int process;
    int requestsRemaining;
    int sectorOffset;
    
    long startLocation;
    long endLocation;

    struct DiskRequest* nextRequest;
    struct DiskRequest* prevRequest;

    void* buffer;
} DiskRequest;
*/

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
    DiskState* disk = &disks[unit];

    while (1) {
        USLOSS_DeviceInput(USLOSS_DISK_DEV, unit, &status);
        waitDevice(USLOSS_DISK_DEV, unit, &status);
        disk->status = status;
        if (status == USLOSS_DEV_READY) {
            unblockProc(curReqs[unit]->process);
        }
        else if (status == USLOSS_DEV_BUSY) {
            unblockProc(curReqs[unit]->process);
        }
        else if (status == USLOSS_DEV_ERROR) {
            unblockProc(curReqs[unit]->process);
            // wake up process and return the status
        }
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
