#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "phase1.h"

// NEED A USLOSSSCONTEXT STRUCT, STORES STATE OF PROCESS
typedef struct PCB {
    int pid;
    int runState;  // WHY DID I PUT RUN STATE???
    int priority;

    char isDead;
    char isForked;
    char processName[MAXNAME];

    struct PCB* parent;
    struct PCB* child;
    struct PCB* prevSibling;
    struct PCB* nextSibling;

    USLOSS_Context context;
} PCB;

PCB processes[MAXPROC];

int currentPID = 1;

/* ---------- Prototypes ---------- */

void initMain();



/* ---------- Phase 1a Functions ---------- */

void phase1_init(void) {
    memset(processes, 0, MAXPROC * sizeof(PCB));
}

void startProcesses(void) {
    // Create init PCB and populate fields
    PCB init;
    processes[currentPID++] = init;

    init.pid = currentPID;
    init.priority = 6;
    strcpy(init.processName, "init");

    // allocate stack, initialize context, and context switch to init
    void* stackMem = malloc(USLOSS_MIN_STACK);
    USLOSS_ContextInit(&init.context, stackMem, USLOSS_MIN_STACK, NULL, &initMain);
    USLOSS_ContextSwitch(NULL, &init.context);
}

int fork1(char *name, int (*func)(char*), char *arg, int stacksize, int priority) {
    if (stacksize < USLOSS_MIN_STACK) {
        return -2;
    }
    if (priority < 1 || priority > 7) {
        return -1;
    }
    if (func == NULL || arg == NULL) {
        return -1;
    }
    if (strlen(arg) > MAXNAME) {
        return -1;
    }
    return 0;
}

int join(int *status) {
    return 0;
}

void quit(int status, int switchToPid) {
    while(1){}
}

int getpid(void) {
    return 0;
}

void dumpProcesses(void) {

}

void TEMP_switchTo(int pid) {

}


/* ---------- Process Functions ---------- */

void initMain() {

    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();

    // create sentinel and testcase_main, switch to testcase_main
    /*
    while (1) {
        join();
    }
    */
}
