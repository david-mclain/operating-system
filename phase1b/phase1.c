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

#define NUMPRIORITIES   7
#define MAX_TIME_SLICE 80000

// run states
#define RUNNABLE    0
#define RUNNING     1
#define BLOCKED     2
#define DEAD        3

// block reasons (ie, why is this process blocked?)
#define UNBLOCKED   0
#define ZAPPING     21
#define JOINING     22

typedef struct PCB {
    int pid;
    int priority;
    int status;

    char arg[MAXARG];
    char processName[MAXNAME];

    int (*processMain)(char*);
    void* stackMem;
    USLOSS_Context context;

    char isAllocated;
    char runState;
    int blockReason;
    
    int currentTimeSlice;
    int totalCpuTime;

    struct PCB* parent;
    struct PCB* child;
    struct PCB* prevSibling;
    struct PCB* nextSibling;

    struct PCB* zappedBy;       // head of list of procs currently zap()-ing this proc
    struct PCB* nextZapper;     // next (after this) in list of procs zap()-ing some OTHER proc

    struct PCB* prevInQueue;    // Prev process in queue that has the same priority
    struct PCB* nextInQueue;    // Next process in queue that has the same priority
} PCB;

typedef struct Queue {
    PCB* head;
    PCB* tail;
} Queue;


    /* ---------- Globals ---------- */

PCB processes[MAXPROC];
PCB* currentProc;       // currently running process
int currentPID = 1;     // next available PID

Queue queues[NUMPRIORITIES]; // queues for dispatcher


    /* ---------- Prototypes ---------- */

int disableInterrupts();
int sentinelMain();
int testcaseMainMain();

void addToQueue();
void checkMode();
void dispatch();
void initMain();
void trampoline();
void removeFromQueue();
void restoreInterrupts();

static void clockHandler(int,void*);
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
    memset(queues, 0, sizeof(queues));
    USLOSS_IntVec[USLOSS_CLOCK_INT] = &clockHandler;
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
    strcpy(init->processName, "init");
    init->isAllocated = 1;

    // allocate stack, initialize context 
    void* stackMem = malloc(USLOSS_MIN_STACK);
    init->stackMem = stackMem;
    USLOSS_ContextInit(&init->context, stackMem, USLOSS_MIN_STACK, NULL, &initMain);
    
    init->runState = RUNNABLE;
    addToQueue(init);
    
    // call dispatcher to switch to init
    dispatch();
    restoreInterrupts(prevInt);
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
    func == NULL || name == NULL || strlen(name) > MAXNAME) {
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
    // set values in struct for new process
    new->pid = currentPID++;
    new->priority = priority;
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
        strcpy(new->arg, arg);
    }

    // allocate stack, initialize context
    void* stackMem = malloc(stacksize);
    new->stackMem = stackMem;
    USLOSS_ContextInit(&new->context, stackMem, stacksize, NULL, &trampoline);

    new->runState = RUNNABLE;
    addToQueue(new);

    dispatch();
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
    //printQueues();
    int prevInt = disableInterrupts();
    
    if (currentProc->child == NULL) { return -2; } // current proc. has no unjoined children
                                                   
    PCB* currChild = currentProc->child;
    while (currChild) {
        if (currChild->runState == DEAD) {
            *status = currChild->status; // collect status

            // remove child from the linked list
            if (currChild->prevSibling != NULL){
                // change left sibling's ptr
                currChild->prevSibling->nextSibling = currChild->nextSibling;
            }
            else {
                // change parent's child ptr
                if (currChild->nextSibling != NULL) {
                    currChild->nextSibling->prevSibling = NULL;
                }
                currentProc->child = currChild->nextSibling; 
            }

            if (currChild->nextSibling != NULL) {
                // change right sibling's ptr
                currChild->nextSibling->prevSibling = currChild->prevSibling;
            } 

            // free up child's stack and clear pointers
            free(currChild->stackMem);
            currChild->isAllocated = 0;
            // might not need to do this, test and see l8r
            currChild->parent = NULL;
            currChild->prevSibling = NULL;
            currChild->nextSibling = NULL;
            currChild->child = NULL;

            restoreInterrupts(prevInt);
            return currChild->pid;
        }
        currChild = currChild->nextSibling;
    }

    // no dead children found; block and wait for one to die
    blockMe(JOINING);

    // now awakened, recursively call join() to collect status
    restoreInterrupts(prevInt);
    return join(status);
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
    removeFromQueue(currentProc);

    PCB* cur = currentProc->zappedBy;
    PCB* temp;
    while (cur) {
        cur->runState = RUNNABLE;
        cur->blockReason = UNBLOCKED;
        addToQueue(cur);

        temp = cur->nextZapper;
        cur->nextZapper = NULL;
        cur = temp;
    }

    PCB* parent = currentProc->parent;
    if (parent->runState == BLOCKED && parent->blockReason == JOINING) {
        parent->runState = RUNNABLE;
        parent->blockReason = UNBLOCKED;
        addToQueue(parent);
    }

    dispatch();
    restoreInterrupts(prevInt);
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
    return currentProc->pid;
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
        if (!processes[i].isAllocated) { continue; }

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
            case BLOCKED:
                USLOSS_Console("Blocked");
                switch (cur->blockReason) {
                    case ZAPPING:
                        USLOSS_Console("(waiting for zap target to quit)");
                        break;
                    case JOINING:
                        USLOSS_Console("(waiting for child to quit)");
                        break;
                    default:
                        USLOSS_Console("(%d)", cur->blockReason);
                }
                break;
            case DEAD:
                USLOSS_Console("Terminated(%d)", cur->status);
                break;
        }
        USLOSS_Console("\n");
    }
    restoreInterrupts(prevInt);
}

    /* -------- Phase 1b Functions -------- */

/**
 * Purpose:
 * Tells a process that it should quit as soon as possible; similar to UNIX kill
 * 
 * Parameters:
 * int pid  process pid that we want to zap
 *
 * Return:
 * None
 */ 
void zap(int pid) {
    checkMode("zap");
    int prevInt = disableInterrupts();

    // check for invalid pid
    char err[] = "ERROR: Attempt to zap()";
    if (pid <= 0) {
        USLOSS_Console("%s a PID which is <=0.  other_pid = %d\n", err, pid);
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
    if (!processes[pid % MAXPROC].isAllocated || processes[pid % MAXPROC].pid != pid) {
        USLOSS_Console("%s a non-existent process.\n", err);
        USLOSS_Halt(1);
    }
    if (processes[pid % MAXPROC].runState == 3) {
        USLOSS_Console("%s a process that is already in the process of dying.\n", err);
        USLOSS_Halt(1);
    }

    // add self to list of processes currently zap()-ing process pid
    PCB* toZap = &processes[pid % MAXPROC]; // do this for every ref. to processes?
    currentProc->nextZapper = toZap->zappedBy;
    toZap->zappedBy = currentProc;

    // block and call dispatcher
    blockMe(ZAPPING);

    restoreInterrupts(prevInt);
}

/**
 * Purpose:
 * Checks to see if the current process is being zapped
 * 
 * Parameters:
 * None
 *
 * Return:
 * int  1 if process is being zapped and 0 otherwise
 */ 
int isZapped(void) {
    checkMode("isZapped");
    return (currentProc->zappedBy != NULL);
}

/**
 * Purpose:
 * Puts the current process into the blocked state
 * 
 * Parameters:
 * int blockStatus  status as to why process is being blocked
 *
 * Return:
 * None
 */ 
void blockMe(int blockStatus) {
    checkMode("blockMe");
    int prevInt = disableInterrupts();

    if (blockStatus <= 10) {
        USLOSS_Console("ERROR: invalid block_status\n");
    }

    currentProc->runState = BLOCKED;
    currentProc->blockReason = blockStatus;
    removeFromQueue(currentProc);
    dispatch();

    restoreInterrupts(prevInt);
}

/**
 * Purpose:
 * Puts a specified process back into the runnable state
 * 
 * Parameters:
 * int pid  pid of process to put back into runnable state
 *
 * Return:
 * int  0 if there were no issues, -2 if there were any issues with unblocking
 */ 
int unblockProc(int pid) {
    checkMode("unblockProc");
    int prevInt = disableInterrupts();

    PCB* proc = &processes[pid % MAXPROC];
    if (proc == NULL || proc->pid != pid) {
        restoreInterrupts(prevInt);
        return -2;
    }
    proc->runState = RUNNABLE;
    proc->blockReason = UNBLOCKED;
    addToQueue(proc);
    dispatch();

    restoreInterrupts(prevInt);
    return 0;
}

/**
 * Purpose:
 * Returns the time the current process began on the CPU in milliseconds
 * 
 * Parameters:
 * None
 *
 * Return:
 * int  time the process started on the system
 */ 
int readCurStartTime(void) {
    return currentProc->currentTimeSlice;
}

/**
 * Purpose:
 * Returns the total time a process has taken on the CPU in microseconds
 * 
 * Parameters:
 * None
 *
 * Return:
 * int  total time a process has been on the CPU
 */ 
int readtime(void) {
    return currentProc->totalCpuTime + (currentTime() - readCurStartTime());
}

/**
 * Purpose:
 * Returns the current time of the system in microseconds
 * 
 * Parameters:
 * None
 *
 * Return:
 * int  current time of system in microseconds
 */ 
int currentTime(void) {
    int ret;
    USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &ret);
    return ret;
}

/**
 * Purpose:
 * Responsible for checking if the current processes time slice has expired.
 * If time slice expired, calls the dispatcher
 * 
 * Parameters:
 * None
 *
 * Return:
 * None
 */ 
void timeSlice(void) {
    if (currentTime() - readCurStartTime() >= MAX_TIME_SLICE) {
        dispatch();
    }
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
    int status = (*currentProc->processMain)(currentProc->arg);
    //USLOSS_Console("Process %d's main function returned %d\n", currentProc->pid,
    //        status);
    quit(status);
}

/**
 * Purpose:
 * Responsible for choosing which process to run next. Performs round robin with
 * 80 millisecond quantum time slicing to choose which process to run next
 * 
 * Parameters:
 * None
 *
 * Return:
 * None
 */ 
void dispatch() {
    int prevInt = disableInterrupts();

    PCB* new;
    int curCpuTime;
    if (currentProc) {
        curCpuTime = currentTime() - readCurStartTime();
    }
    else {
        curCpuTime = 0;
    }
    for (int i = 0; i < NUMPRIORITIES; i++) {
        Queue* q = &queues[i];
        // Checking if a current process is running and if it's time slice is up add to queue
        if (currentProc && i == currentProc->priority - 1 && currentProc->runState == RUNNING) {
            if (curCpuTime >= MAX_TIME_SLICE) {
                currentProc->runState = RUNNABLE;
                currentProc->totalCpuTime = currentProc->totalCpuTime + curCpuTime;
                addToQueue(currentProc);
            }
            else {
                return;
            }
        }
        // If current queue has a process available
        if (q->head != NULL) {
            // If there is a process currently running, put it in run queue
            if (currentProc && currentProc->runState == RUNNING) {
                currentProc->runState = RUNNABLE;
                currentProc->totalCpuTime = currentProc->totalCpuTime + curCpuTime;
                addToQueue(currentProc);
            }
            new = q->head;
            removeFromQueue(new);
            break;
        }
    }
    // Set current proc to be dispatchers choice
    PCB* oldProc = currentProc;
    currentProc = new;
    currentProc->runState = RUNNING;
    int x;
    USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &x);
    currentProc->currentTimeSlice = x;
    restoreInterrupts(prevInt);
    if (oldProc) {
        USLOSS_ContextSwitch(&(oldProc->context), &(new->context));
    }
    else {
        USLOSS_ContextSwitch(NULL, &(new->context));
    }
}

/**
 * Purpose:
 * Responsible for adding a process to its respective run queue
 * 
 * Parameters:
 * PCB* process     process to add to run queue
 *
 * Return:
 * None
 */ 
void addToQueue(PCB* process) {
    Queue* addTo = &queues[(process->priority)-1];
    if (addTo->head == NULL || addTo->tail == NULL) {
        addTo->head = process;
        addTo->tail = process;
    }
    else {
        addTo->tail->nextInQueue = process;
        process->prevInQueue = addTo->tail;
        addTo->tail = process;
    }
}

/**
 * Purpose:
 * Responsible for removing a process from its run queue
 * 
 * Parameters:
 * PCB* process     process to remove from the run queue
 *
 * Return:
 * None
 */ 
void removeFromQueue(PCB* process) {
    // change the queue's head and/or tail pointer(s) if applicable
    Queue* removeFrom = &queues[(process->priority)-1];
    if (removeFrom->head == process) { 
        removeFrom->head = process->nextInQueue;
    }
    if (removeFrom->tail == process) {
        removeFrom->tail = process->prevInQueue;
    }

    // connect pointers for remaining elements in the queue
    if (process->prevInQueue) {
        process->prevInQueue->nextInQueue = process->nextInQueue;
    }
    if (process->nextInQueue) {
        process->nextInQueue->prevInQueue = process->prevInQueue;
    }
    
    // clean up process's queue pointers
    process->prevInQueue = NULL;
    process->nextInQueue = NULL;
}

/**
 * Purpose:
 * Responsible for handling clock interrupts
 * 
 * Parameters:
 * int dev      idk
 * void* arg    idk
 *
 * Return:
 * None
 */ 
static void clockHandler(int dev, void* arg) {
    phase2_clockHandler();
    timeSlice();
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

    // create sentinel and testcase_main
    int sentinelPid = fork1("sentinel", &sentinelMain, NULL, USLOSS_MIN_STACK, 7);
    int testcaseMainPid = fork1("testcase_main", &testcaseMainMain, NULL, USLOSS_MIN_STACK, 3);
    // continuously clean up dead children
    int childPid, childStatus;
    while (1) {
        childPid = join(&childStatus);
        // maybe do something with status here?
    }
}

/**
 * Purpose:
 * Main function for sentinel process, handles deadlocks
 * 
 * Parameters:
 * char* arg    Arguments for sentinel main function
 *
 * Return:
 * int  Return status of main function (should never return)
 */ 
int sentinelMain(char* arg) {           // david
    while (1) {
        if (phase2_check_io() == 0) {
            USLOSS_Console("DEADLOCK DETECTED!  All of the processes have blocked, but I/O is not ongoing.\n");
            USLOSS_Halt(0);
        }
        USLOSS_WaitInt();
    }
    return 0;
}

/**
 * Purpose:
 * Main function for testcase_main process
 * 
 * Parameters:
 * char* arg    Arguments for testcase_main main function
 *
 * Return:
 * int  Return status of main function
 */ 
int testcaseMainMain(char* arg) {
    int test_return = testcase_main();
    if (test_return != 0) { 
        USLOSS_Console("testcase_main returned with a nonzero error code: %d\n", test_return);
    }
    USLOSS_Halt(test_return);
    return 0;
}
