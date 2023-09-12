#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "phase1.h"

#define RUNNABLE    0
#define RUNNING     1
#define DEAD        2

typedef struct PCB {
    int pid;
    int priority;
    int status;

    char args[MAXARG];
    char processName[MAXNAME];
    char isAllocated;
    char runState;

    struct PCB* parent;
    struct PCB* child;
    struct PCB* prevSibling;
    struct PCB* nextSibling;

    USLOSS_Context context;

    int (*processMain)(char*);

    void* stackMem;
} PCB;

PCB processes[MAXPROC];
PCB* currentProc;       // currently running process
int currentPID = 1;     // next available PID

/* ---------- Prototypes ---------- */

void trampoline();
int startSentinel();

void initMain();
int sentinelMain();
int testcaseMainMain();


/* ---------- Phase 1a Functions ---------- */
// NEED TO DISABLE & RE-ENABLE INTERRUPTS FOR EACH OF THESE FUNCTIONS (except getpid)
// AND CHECK TO ENSURE WE'RE IN KERNEL MODE

// MEMSET NOT WORKING PROPERLY, NEED TO FIX
void phase1_init(void) {
    // TEMPORARY UNTIL FIGURE OUT MEMSET????
    for (int i = 0; i < MAXPROC; i++) {
        processes[i].pid = 0;
        processes[i].priority = 0;
        processes[i].status = 0;
        processes[i].isAllocated = 0;
        processes[i].runState = 0;
        memset(processes[i].args, 0, sizeof(char) * MAXARG);
        memset(processes[i].processName, 0, sizeof(char) * MAXNAME);
        processes[i].parent = NULL;
        processes[i].child = NULL;
        processes[i].prevSibling = NULL;
        processes[i].nextSibling = NULL;
        processes[i].context = (USLOSS_Context){NULL, 0, 0}; 
        processes[i].processMain = NULL;
        processes[i].stackMem = NULL;
    }
    //memset(processes, 0, sizeof(processes));
}

void startProcesses(void) {
    // Create init PCB and populate fields
    PCB* init = &processes[currentPID % MAXPROC];
    init->pid = currentPID++;
    init->priority = 6;
    init->isAllocated = 1;
    strcpy(init->processName, "init");

    // allocate stack, initialize context 
    void* stackMem = malloc(USLOSS_MIN_STACK);
    init->stackMem = stackMem;
    USLOSS_ContextInit(&init->context, stackMem, USLOSS_MIN_STACK, NULL, &initMain); // maybe change to an int main() and use trampoline?

    // Set init as the current running process
    currentProc = init;
    init->runState = RUNNING;

    // context switch to init
    USLOSS_ContextSwitch(NULL, &init->context); // call dispatcher here for 1b
}

int fork1(char *name, int (*func)(char*), char *arg, int stacksize, int priority) {
    if (stacksize < USLOSS_MIN_STACK) {
        return -2;
    }
    if (priority < 1 || (priority > 5 && strcmp(name, "sentinel") != 0 && priority != 7) ||
            func == NULL || name == NULL || strlen(name) > MAXNAME) 
    {
        return -1;
    }
    int i = 0;
    for (; i < MAXPROC && processes[currentPID % MAXPROC].isAllocated; i++) {
        currentPID++;
    }
    if (i == MAXPROC) {
        return -1;
    }

    PCB* new = &processes[currentPID % MAXPROC];
    new->pid = currentPID;
    new->priority = priority;
    new->runState = RUNNABLE;
    new->isAllocated = 1;
    strcpy(new->processName, name);
    new->parent = currentProc;
    if (currentProc->child != NULL) {
        new->nextSibling = currentProc->child;
        currentProc->child->prevSibling = new;
    }
    currentProc->child = new;

    new->processMain = func;
    if (arg != NULL) {
        strcpy(new->args, arg);
    }
    // allocate stack, initialize context, and context switch to init
    void* stackMem = malloc(stacksize);
    new->stackMem = stackMem;
    USLOSS_ContextInit(&new->context, stackMem, stacksize, NULL, &trampoline);
    
    return new->pid;
}

int join(int *status) {
    if (currentProc->child == NULL) { return -2; } // current proc. has no unjoined children
    else {
        PCB* currChild = currentProc->child;
        while (currChild) {
            if (currChild->runState == DEAD) {

                // collect status
                *status = currChild->status; // run status vs. exit status?


                // remove child from the linked list
                
                if (currChild->prevSibling != NULL){    // change left sibling's ptr
                    currChild->prevSibling->nextSibling = currChild->nextSibling;
                }
                else {  // change parent's child ptr
                    if (currChild->nextSibling != NULL) {
                        currChild->nextSibling->prevSibling = NULL;
                    }
                    currentProc->child = currChild->nextSibling; 
                }

                if (currChild->nextSibling != NULL) {   // change right sibling's ptr
                    currChild->nextSibling->prevSibling = currChild->prevSibling;
                } 
                

                // free up child's stack and clear pointers
                free(currChild->stackMem);
                currChild->isAllocated = 0;
                currChild->parent = NULL;
                currChild->prevSibling = NULL;
                currChild->nextSibling = NULL;
                currChild->child = NULL;

                return currChild->pid;
            }
            currChild = currChild->nextSibling;
        }

        return 0; // in 1b block here, but shouldn't ever get here in 1a
    }
}

void quit(int status, int switchToPid) {
    currentProc->status = status;
    currentProc->runState = DEAD;
    TEMP_switchTo(switchToPid);
}

int getpid(void) {
    if (currentProc) { return currentProc->pid; }
    else { return -1; }  // is this good? return something else? ask
}

void dumpProcesses(void) {
    USLOSS_Console(" PID  PPID  NAME              PRIORITY  STATE\n");
    for (int i = 0; i < MAXPROC; i++) {
        if (processes[i].isAllocated) {
            PCB* cur = &processes[i];

            USLOSS_Console("%4d  ", cur->pid);

            if (cur->parent != NULL) { USLOSS_Console("%4d  ", cur->parent->pid); }
            else { USLOSS_Console("   0  "); }

            USLOSS_Console("%-16s  ", cur->processName);

            //if (cur->child != NULL) { USLOSS_Console("%8d  ", cur->child->pid); }

            USLOSS_Console("%-8d  ", cur->priority);
            switch (cur->runState) {
                case RUNNABLE:
                    USLOSS_Console("Runnable");
                    break;
                case RUNNING:
                    USLOSS_Console("Running");
                    break;
                case DEAD:
                    USLOSS_Console("Terminated(%d)", cur->status);
                    break;
            }
            USLOSS_Console("\n");
        }
    }
}

void TEMP_switchTo(int pid) {
    PCB* switchTo = &processes[pid % MAXPROC];
    USLOSS_Context* prev_context = &(currentProc->context);
    if (currentProc->runState == RUNNING) { currentProc->runState=RUNNABLE; }
    switchTo->runState = RUNNING;
    currentProc = switchTo;
    USLOSS_ContextSwitch(prev_context, &(switchTo->context)); // ERROR HERE GETTING SEG FAULT
}

/* ---------- Helper Functions ---------- */

void trampoline() {
    (*currentProc->processMain)(currentProc->args);
}

int startSentinel() {
    PCB new;
    new.pid = currentPID;
    new.priority = 7;
    new.isAllocated = 1;
    new.status = RUNNABLE;
    strcpy(new.processName, "sentinel");
    new.parent = currentProc;
    if (currentProc->child != NULL) {
        new.nextSibling = currentProc->child;
        currentProc->child->prevSibling = &new;
    }
    // allocate stack, initialize context, and context switch to init
    void* stackMem = malloc(USLOSS_MIN_STACK);
    new.stackMem = stackMem;
    USLOSS_ContextInit(&new.context, stackMem, USLOSS_MIN_STACK, NULL, &trampoline);
    processes[new.pid % MAXPROC] = new;    
    return new.pid;
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
    int sentinelPid = fork1(sen, &sentinelMain, NULL, USLOSS_MIN_STACK, 3);
    int testcaseMainPid = fork1(test, &testcaseMainMain, NULL, USLOSS_MIN_STACK, 3);

    USLOSS_Console("Phase 1A TEMPORARY HACK: init() manually switching to testcase_main() after using fork1() to create it.\n");
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
    USLOSS_Console("Phase 1A TEMPORARY HACK: testcase_main() returned, simulation will now halt.\n");
    if (test_return != 0) { 
        USLOSS_Console("testcase_main returned with a nonzero error code: %d\n", test_return); // do this for all printouts
    }
    USLOSS_Halt(test_return); // pass zero? ask about this
    return 0;
}
