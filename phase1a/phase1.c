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
    char isAllocated;
    char processName[MAXNAME];

    struct PCB* parent;
    struct PCB* child;
    struct PCB* prevSibling;
    struct PCB* nextSibling;

    USLOSS_Context context;
} PCB;

PCB processes[MAXPROC];
PCB* currentProc;
int currentPID = 1;

/* ---------- Prototypes ---------- */

void initMain();
int sentinelMain();
int testcaseMainMain();



/* ---------- Phase 1a Functions ---------- */

void phase1_init(void) {
    memset(processes, 0, MAXPROC * sizeof(PCB));
}

void startProcesses(void) {
    // Create init PCB and populate fields
    PCB init;
    init.pid = currentPID;
    init.priority = 6;
    init.isAllocated = 1;
    strcpy(init.processName, "init");
    processes[currentPID++] = init;

    // Set init as the current running process
    currentProc = &init;    // maybe move later

    // allocate stack, initialize context, and context switch to init
    void* stackMem = malloc(USLOSS_MIN_STACK);
    USLOSS_ContextInit(&init.context, stackMem, USLOSS_MIN_STACK, NULL, &initMain);
    USLOSS_ContextSwitch(NULL, &init.context); // call dispatcher here for 1b
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
    PCB new;
    int i = 0;
    for (; i < MAXPROC && processes[currentPID % MAXPROC].isAllocated; i++) {
        currentPID++;
    }
    if (i == MAXPROC) {/*DO SOMETHING*/}
    new.pid = currentPID;
    new.priority = priority;
    new.isAllocated = 1;
    strcpy(new.processName, name);

    void trampoline() {
        (*func)(arg);
    }

    // allocate stack, initialize context, and context switch to init
    void* stackMem = malloc(stacksize);
    USLOSS_ContextInit(&new.context, stackMem, stacksize, NULL, &trampoline);
    
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
    char sen[] = "sentinel";            // maybe change l8r, enforce length or smth
    char test[] = "testcase_main";
    int sentinelPid = fork1(sen, &sentinelMain, NULL, USLOSS_MIN_STACK, 7);           // stack size?? how to choose
    int testcaseMainPid = fork1(test, &testcaseMainMain, NULL, USLOSS_MIN_STACK, 3);
    TEMP_switchTo(testcaseMainPid);     // call dispatcher here in 1b

    /*
    while (1) {
        join();
    }
    */
}

int sentinelMain(char* arg) {
    return 0;
}

int testcaseMainMain(char* arg) {
    int test_return = testcase_main();
    if (test_return != 0) { 
        printf("testcase_main returned with a nonzero error code: %d\n", test_return);
    }
    USLOSS_Halt(test_return); // pass zero? ask about this
    return 0;
}


