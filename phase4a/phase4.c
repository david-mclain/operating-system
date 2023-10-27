#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase3_usermode.h>
#include <phase4.h>
#include <phase4_usermode.h>
#include <string.h>
#include <usloss.h>

#define LEFT(x) 2 * x + 1
#define RIGHT(x) 2 * x + 2
#define PARENT(x) x / 2

#define SEC_TO_SLEEP_CYCLE(x) x * 10;

typedef struct PCB {
    int pid;
    int sleepCyclesRemaining;
    // idk something something blah blah
} PCB;

void swap(PCB*, PCB*);
void sink(int);
void swim(int);
void cleanHeap();
void remove();
void insert(PCB*);

void kernelSleep(USLOSS_Sysargs*);

int daemonMain(char*);


PCB processes[MAXPROC];
PCB sleepHeap[MAXPROC];
int elementsInHeap = 0;

int sleepDaemon;

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

    systemCallVec[SYS_SLEEP] = kernelSleep;
}

void phase4_start_service_processes() {
    sleepDaemon = fork1("Sleep Daemon", daemonMain, NULL, USLOSS_MIN_STACK, 1);
}

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
    blockMe(pid);
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

}

int daemonMain(char* args) {
    int status;
    while (1) {
        waitDevice(USLOSS_CLOCK_INT, 0, &status);
        cleanHeap();
    }
    return 0; // shouldn't get here?
}

// decrement all sleepCyclesRemaining in heap, remove if needed
// while sleepHeap[0].cycles = 0 remove and unblock
void cleanHeap() {
    for (int i = 0; i < elementsInHeap; i++) {
        sleepHeap[i].sleepCyclesRemaining--;
    }
    while (elementsInHeap > 0 && sleepHeap[0].sleepCyclesRemaining == 0) {
        remove();
    }
}

void insert(PCB* proc) {
    int curIndex = elementsInHeap++;
    PCB* cur = &sleepHeap[curIndex];
    memcpy(cur, proc, sizeof(PCB));
    swim(curIndex);
}

void remove() {
    PCB cur = sleepHeap[0];
    elementsInHeap--;
    memcpy(&sleepHeap[0], &sleepHeap[elementsInHeap], sizeof(PCB));
    sink(0);
    unblockProc(cur.pid);
}

void swim(int index) {
    int i = index, j = PARENT(index);
    while (j > 0) {
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

