#include "phase1.h"
#include <stddef.h>
#include <string.h>

// GOOGLE MEMSET FOR WIPING BIG DATA STRUCTURE
// NEED A USLOSSSCONTEXT STRUCT, STORES STATE OF PROCESS

typedef struct PCB {
    int pid;
    int runState;  // WHY DID I PUT RUN STATE???
    int priority;

    PCB* parent;
    PCB* child;
    PCB* sibling;

    char processName[MAXNAME];

    USLOSS_Context context;
} PCB;

PCB processes[MAXPROC];

int currentPID = 1;

// ContextSwitch(old, new);
// old is what it was, new is what it currently is
// WHEN SWITCHING CONTEXTS MAKE SURE TO SAVE STATE

void phase1_init(void) {
    PCB init;

    init.pid = currentPID;
    init.priority = 6;

    init->parent = NULL;
    init->child = NULL;
    init->sibling = NULL;

    strcpy(init.processName, "init");
    processes[currentPID++] = init;
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

void startProcesses(void) {

}

void TEMP_switchTo(int pid) {

}
