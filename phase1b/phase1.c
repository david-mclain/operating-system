/**
 * File: phase1.c
 * Authors: David McLain, Miles Gendreau
 *
 * Purpose: phase1.c contains function implementations for 
 * the very first steps of simulating an operating system.
 * Creates process table and PCB data types and manages starting
 * different types of processes, joining and quitting them.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "phase1.h"

#define RUNNABLE    0
#define RUNNING     1
#define BLOCKED     2
#define DEAD        3

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

    struct PCB* zappedBy;   // head of list of procs currently zap()-ing this proc
    struct PCB* nextZapper; // next (after this) in list of procs zap()-ing some OTHER proc

    USLOSS_Context context;

    int (*processMain)(char*);

    void* stackMem;
} PCB;

PCB processes[MAXPROC];
PCB* currentProc;       // currently running process
int currentPID = 1;     // next available PID


    /* ---------- Prototypes ---------- */

void trampoline();

void checkMode(char* fnName);
int disableInterrupts();
void restoreInterrupts(int prevInt);

void dispatch();

void initMain();
int sentinelMain();
int testcaseMainMain();

    /* ---------- Phase 1a Functions ---------- */

/**
 * Purpose:
 * Initializes PCB data structure for OS simulation
 * 
 * Parameters:
 * None
 *
 * Return:
 * None
 */ 
void phase1_init(void) {
    memset(processes, 0, sizeof(processes));
}

/**
 * Purpose:
 * Creates the init process, then context switches to init
 * 
 * Parameters:
 * None
 *
 * Return:
 * None
 */ 
void startProcesses(void) {
    checkMode("startProcesses");
    int prevInt = disableInterrupts();

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

    // restore interrupts and context switch to init
    restoreInterrupts(prevInt);
    // CALL DISPATCHER HERE
    USLOSS_ContextSwitch(NULL, &init->context); // call dispatcher here for 1b
}

/**
 * Purpose:
 * Creates a new process and fills in all PCB fields for it
 * 
 * Parameters:
 * char* name           Name for the new process to create
 * int (*func)(char*)   Function pointer to new processes main function
 * char* arg            Arguments to pass into processes main function
 * int stacksize        Size of stack for process
 * int priority         Priority to set for process
 *
 * Return:
 * int  PID of new process
 */ 
int fork1(char *name, int (*func)(char*), char *arg, int stacksize, int priority) {
    checkMode("fork1");
    int prevInt = disableInterrupts();

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
    new->pid = currentPID++;
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

    // allocate stack, initialize context
    void* stackMem = malloc(stacksize);
    new->stackMem = stackMem;
    USLOSS_ContextInit(&new->context, stackMem, stacksize, NULL, &trampoline);

    restoreInterrupts(prevInt);
    return new->pid;
}

/**
 * Purpose:
 * Cleans up zombie children processes
 * 
 * Parameters:
 * int* status - status of the child process being joined
 *
 * Return:
 * int - PID of child process that was joined
 */ 
int join(int *status) {
    checkMode("join");
    int prevInt = disableInterrupts();
    
    if (currentProc->child == NULL) { return -2; } // current proc. has no unjoined children
                                                   
    PCB* currChild = currentProc->child;
    while (currChild) {
        if (currChild->runState == DEAD) {

            *status = currChild->status; // collect status

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

            restoreInterrupts(prevInt);
            return currChild->pid;
        }
        currChild = currChild->nextSibling;
    }

    restoreInterrupts(prevInt);
    return 0; // in 1b block here, but shouldn't ever get here in 1a
}

/**
 * Purpose:
 * Makes a process a zombie
 * 
 * Parameters:
 * int status - status of process to make zombie
 *
 * Return:
 * None
 */ 
void quit(int status) {
    checkMode("quit");
    int prevInt = disableInterrupts();
    
    if (currentProc->child) {
        USLOSS_Console("ERROR: Process pid %d called quit() while it still had children.\n", currentProc->pid);
        USLOSS_Halt(1);
    }
    currentProc->status = status;
    currentProc->runState = DEAD;

    restoreInterrupts(prevInt);
    // CALL DISPATCHER HERE
}

/**
 * Purpose:
 * Returns PID of current running process
 * 
 * Parameters:
 * None
 *
 * Return:
 * int - PID of current running process
 */ 
int getpid(void) {
    checkMode("getpid");

    if (currentProc) { return currentProc->pid; }
    else { return -1; }  // is this good? return something else? ask
}

/**
 * Purpose:
 * Dumps out information on all running or zombies processes
 * 
 * Parameters:
 * None
 *
 * Return:
 * None
 */ 
void dumpProcesses(void) {
    checkMode("dumpProcesses");
    int prevInt = disableInterrupts();
    
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
    restoreInterrupts(prevInt);
}

    /* -------- Phase 1b Functions -------- */

void zap(int pid) {
    checkMode("zap");
    int prevInt = disableInterrupts();

    // check for invalid pid
    char err[] = "ERROR: Attempt to zap()";
    if (pid <= 0) {
        USLOSS_Console("%s a PID which is <=0. other_pid = %d\n", err, pid);
        USLOSS_Halt(1);
    }
    if (pid == 1) {
        USLOSS_Console("%s init.\n", err);
        USLOSS_Halt(1);
    }
    if (pid == currentProc->pid) {
        USLOSS_Console("%s itself.\n", err);
        USLOSS_Halt(1);
    }
    if (!processes[pid % MAXPROC].isAllocated) {
        USLOSS_Console("%s a non-existent process.\n", err);
        USLOSS_Halt(1);
    }
    // USLOSS_Console("%s a process that is already in the process of dying.\n", err);
    // not sure what conditions would apply for this one, but saw it in testcases

    // add self to list of processes currently zap()-ing process pid
    PCB* toZap = &processes[pid % MAXPROC]; // do this for every ref. to processes?
    currentProc->nextZapper = toZap->zappedBy;
    toZap->zappedBy = currentProc;

    // block and call dispatcher
    currentProc->runState = BLOCKED;
    restoreInterrupts(prevInt); // ?
    dispatch();
}

int isZapped(void) {
    return (currentProc->zappedBy != NULL);
}

void blockMe(int block_status) {

}

int unblockProc(int pid) {

}

int readCurStartTime(void) {

}

void timeSlice(void) {

}

int readtime(void) {

}

int currentTime(void) {

}

    /* ---------- Helper Functions ---------- */

/**
 * Purpose:
 * Checks the current mode of the operating system, halts if user mode attempts to
 * execute kernel functions
 * 
 * Parameters:
 * char* fnName - Name of function that user mode process attempted to execute
 *
 * Return:
 * None
 */ 
void checkMode(char* fnName) {
    if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) {
        USLOSS_Console("ERROR: Someone attempted to call %s while in user mode!\n",
                fnName);
        USLOSS_Halt(1);
    }
}

/**
 * Purpose:
 * Disables interrupts for when executing kernel mode functions
 * 
 * Parameters:
 * None
 *
 * Return:
 * int - State that interrupts were in before function call
 */ 
int disableInterrupts() {
    int prevInt = USLOSS_PsrGet() & USLOSS_PSR_CURRENT_INT; // previous interrupt status
    USLOSS_PsrSet(prevInt & ~USLOSS_PSR_CURRENT_INT);
    return prevInt;
}

/**
 * Purpose:
 * Restores interrupts after kernel function has finished executing
 * 
 * Parameters:
 * int prevInt - State that interrupts were in before function call
 *
 * Return:
 * None
 */ 
void restoreInterrupts(int prevInt) {
    USLOSS_PsrSet(USLOSS_PsrGet() | prevInt);
}

/**
 * Purpose:
 * Trampoline function used to bounce current process start function to its
 * main function
 * 
 * Parameters:
 * None
 *
 * Return:
 * None
 */ 
void trampoline() {
    // enable interrupts
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    (*currentProc->processMain)(currentProc->args);
    // do something (call quit? halt?) here
}

void dispatch() {
    // dispatch here ez pz lemon squeezy
    USLOSS_Console("dispatcher: Not implemented yet :(");
}


    /* ---------- Process Functions ---------- */

/**
 * Purpose:
 * Main function for init process, handles starting other service processes
 * and forking other processes to get test case and sentinel running
 * 
 * Parameters:
 * None
 *
 * Return:
 * None
 */ 
void initMain() {
    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();

    // create sentinel and testcase_main, switch to testcase_main
    char sen[] = "sentinel";            // maybe change l8r, enforce length or smth
    char test[] = "testcase_main";
    int sentinelPid = fork1(sen, &sentinelMain, NULL, USLOSS_MIN_STACK, 7);
    int testcaseMainPid = fork1(test, &testcaseMainMain, NULL, USLOSS_MIN_STACK, 3);

    USLOSS_Console("Phase 1A TEMPORARY HACK: init() manually switching to testcase_main() after using fork1() to create it.\n");
    /*
    while (1) {
        join();
    }
    */
}

/**
 * Purpose:
 * Main function for sentinel process, handles deadlocks
 * 
 * Parameters:
 * char* args - Arguments for sentinel main function
 *
 * Return:
 * int - Return status of main function (should never return)
 */ 
int sentinelMain(char* args) {           // david
    return 0;
}

/**
 * Purpose:
 * Main function for testcase_main process
 * 
 * Parameters:
 * char* args - Arguments for testcase_main main function
 *
 * Return:
 * int - Return status of main function
 */ 
int testcaseMainMain(char* arg) {
    int test_return = testcase_main();
    USLOSS_Console("Phase 1A TEMPORARY HACK: testcase_main() returned, simulation will now halt.\n");
    if (test_return != 0) { 
        USLOSS_Console("testcase_main returned with a nonzero error code: %d\n", test_return);
    }
    USLOSS_Halt(test_return); // pass zero? ask about this
    return 0;
}
