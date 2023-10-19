#include <phase1.h>
#include <phase2.h>
#include <phase3_usermode.h>
#include <stdio.h>
#include <string.h>
#include <usloss.h>

#define USER_MODE 0x02

typedef struct PCB {
    int (*main)(char*);
} PCB;

void kernelSemCreate(USLOSS_Sysargs*);
void kernelSemP(USLOSS_Sysargs*);
void kernelSemV(USLOSS_Sysargs*);
void kernelSpawn(USLOSS_Sysargs*);
void kernelTerminate(USLOSS_Sysargs*);
void kernelWait(USLOSS_Sysargs*);

void kernelGetTimeOfDay(USLOSS_Sysargs*);
void kernelCPUTime(USLOSS_Sysargs*);
void kernelGetPid(USLOSS_Sysargs*);
int trampoline();

PCB processes[MAXPROC];

void phase3_init(void) {
    systemCallVec[SYS_SPAWN] = kernelSpawn;
    systemCallVec[SYS_WAIT] = kernelWait;
    systemCallVec[SYS_TERMINATE] = kernelTerminate;
    systemCallVec[SYS_SEMCREATE] = kernelSemCreate;
    systemCallVec[SYS_SEMP] = kernelSemP;
    systemCallVec[SYS_SEMV] = kernelSemV;
    systemCallVec[SYS_GETTIMEOFDAY] = kernelGetTimeOfDay;
    systemCallVec[SYS_GETPROCINFO] = kernelCPUTime;
    systemCallVec[SYS_GETPID] = kernelGetPid;
}

void phase3_start_service_processes() {}

// figure out what function to pass in and how to trampoline properly
// what if we keep track of current main off the start, and then call that in trampoline,
// but if it doesnt context switch then we use the process table main?
//
// so like
//
// if no good idea just use mailboxes to block (lame idea)
// pass param as mbox id? idk change params as necessary ig
// what if we found way to get pc, then just have it jump back to the pc?

void kernelSpawn(USLOSS_Sysargs* args) {
    int len = args->arg2 == NULL ? 0 : strlen(args->arg2) + 1;
    int mbox = MboxCreate(1, len);
    int pid = fork1((char*)args->arg5, trampoline, (char*)(long)mbox, (int)(long)args->arg3, (int)(long)args->arg4);

    if (pid < 0) {
        args->arg4 = -1;
        USLOSS_PsrSet(USER_MODE);
        return;
    }
    PCB* cur = &processes[pid % MAXPROC];
    cur->main = args->arg1;
    args->arg1 = (long)pid;
    args->arg4 = 0;
    MboxSend(mbox, args->arg2, len);
}

int trampoline(char* args) {
    int mbox = (long)(void*)args;
    char funcArgs[MAX_MESSAGE];
    MboxRecv(mbox, funcArgs, MAX_MESSAGE);
    int pid = getpid();

    int (*func)(char*) = processes[pid % MAXPROC].main;

    USLOSS_PsrSet(USER_MODE);

    int ret = (*func)(funcArgs);
    Terminate(ret);
    return 0; // shouldnt ever get here
}

void kernelWait(USLOSS_Sysargs* args) {
    int status;
    int pid;
    pid = join(&status);
    args->arg1 = (void*)(long)pid;
    args->arg2 = (void*)(long)status;
    args->arg4 = status = -2 ? -2 : 0;
    USLOSS_PsrSet(USER_MODE);
}

void kernelTerminate(USLOSS_Sysargs* args) {
    int childPid = 1;
    int status;
    while (childPid != -2) {
        childPid = join(&status);
    }
    args->arg2 = status;
    quit(args->arg1);
}

void kernelSemCreate(USLOSS_Sysargs* args) {

}

void kernelSemP(USLOSS_Sysargs* args) {

}

void kernelSemV(USLOSS_Sysargs* args) {

}


void kernelGetTimeOfDay(USLOSS_Sysargs* args) {

}

void kernelCPUTime(USLOSS_Sysargs* args) {
    args->arg1 = readtime();
    USLOSS_PsrSet(USER_MODE);
}

void kernelGetPid(USLOSS_Sysargs* args) {
    args->arg1 = getpid();
    USLOSS_PsrSet(USER_MODE);
}
