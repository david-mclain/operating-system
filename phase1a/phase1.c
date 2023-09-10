#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "phase1.h"

typedef struct PCB {
    int pid;
    int priority;
    int status;

    char isAllocated;
    char isDead;
    char args[MAXARG];
    char processName[MAXNAME];

    struct PCB* parent;
    struct PCB* child;
    struct PCB* prevSibling;
    struct PCB* nextSibling;

    USLOSS_Context context;

    int (*processMain)(char*);

    void* stackMem;
} PCB;

PCB processes[MAXPROC];
PCB* currentProc;
int currentPID = 1;

/* ---------- Prototypes ---------- */

void trampoline();
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
    if (priority < 1 || priority > 7 || func == NULL || name == NULL || strlen(name) > MAXNAME) {
        return -1;
    }

    PCB new;
    int i = 0;
    for (; i < MAXPROC && processes[currentPID % MAXPROC].isAllocated; i++) {
        currentPID++;
    }
    if (i == MAXPROC) {
        return -1;
    }
    new.pid = currentPID;
    new.priority = priority;
    new.isAllocated = 1;
    strcpy(new.processName, name);
    
    new.parent = currentProc;
    if (currentProc->child != NULL) {
        new.nextSibling = currentProc->child;
        currentProc->child->prevSibling = &new;
    }
    
    // allocate stack, initialize context, and context switch to init
    void* stackMem = malloc(stacksize);
    new.stackMem = stackMem;
    USLOSS_ContextInit(&new.context, stackMem, stacksize, NULL, &trampoline);
    processes[new.pid % MAXPROC] = new;    
    return new.pid;
}

int join(int *status) {     //miles
    return 0;
}

void quit(int status, int switchToPid) {    // david
    while(1){}
}

int getpid(void) {      // miles
    return 0;
}

void dumpProcesses(void) {
    for (int i = 0; i < MAXPROC; i++) {
        if (processes[i].isAllocated) {
            PCB cur = processes[i];
            printf("Process Name: %s", cur.processName);
            printf("Process ID:   %d", cur.pid);
            if (cur.parent != NULL) {
                printf("Parent PID:   %d", cur.parent->pid);
            }
            printf("Priority:     %d", cur.priority);
            printf("Runnable:     %d", cur.status);
        }
    }
}

void TEMP_switchTo(int pid) {       // miles
    
}

void trampoline() {
    (*currentProc->processMain)(currentProc->args);
    if ()
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

int sentinelMain(char* arg) {           // david
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
