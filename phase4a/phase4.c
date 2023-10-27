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

typedef struct PCB {
    int pid;
    int sleepCyclesRemaining;
    // idk something something blah blah
} PCB;

void swap(PCB*, PCB*);
void sink(int);
void swim(int);

PCB sleepHeap[MAXPROC];
int elementsInHeap = 0;

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
}

void phase4_start_service_processes() {

}

int kernSleep(int seconds) {

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

void insert(PCB* proc) {
    int curIndex = elementsInHeap++;
    PCB* cur = &sleepHeap[curIndex];
    memcpy(cur, proc, sizeof(PCB));
    swim(curIndex);
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

