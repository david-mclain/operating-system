#include <phase1.h>
#include <phase2.h>
#include <phase3_usermode.h>
#include <stdio.h>
#include <string.h>
#include <usloss.h>

void kernelSemCreate(USLOSS_Sysargs*);
void kernelSemP(USLOSS_Sysargs*);
void kernelSemV(USLOSS_Sysargs*);
void kernelSpawn(USLOSS_Sysargs*);
void kernelTerminate(USLOSS_Sysargs*);
void kernelWait(USLOSS_Sysargs*);

void kernelGetTimeOfDay(USLOSS_Sysargs*);
void kernelCPUTime(USLOSS_Sysargs*);
void kernelGetPid(USLOSS_Sysargs*);
void trampoline();

void phase3_init(void) {
    systemCallVec[SYS_SPAWN] = &kernelSpawn;
    systemCallVec[SYS_WAIT] = &kernelWait;
    systemCallVec[SYS_TERMINATE] = &kernelTerminate;
    systemCallVec[SYS_SEMCREATE] = &kernelSemCreate;
    systemCallVec[SYS_SEMP] = &kernelSemP;
    systemCallVec[SYS_SEMV] = &kernelSemV;
    systemCallVec[SYS_GETTIMEOFDAY] = &kernelGetTimeOfDay;
    systemCallVec[SYS_GETPROCINFO] = &kernelCPUTime;
    systemCallVec[SYS_GETPID] = &kernelGetPid;
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
int (*func)(char*);
void kernelSpawn(USLOSS_Sysargs* args) {
    func = args->1;
    int pid = fork1(args->arg5, &args->arg1, args->arg2, args->arg3, args->arg4);

    USLOSS_PsrSet(0x6);
    args->arg1 = (long)pid;
    args->arg4 = pid < 0 ? -1 : 0;
}

void trampoline() {
    printf("trampoliney\n");
}

void kernelWait(USLOSS_Sysargs* args) {
    printf("waiting\n");    
}

void kernelTerminate(USLOSS_Sysargs* args) {
    printf("terminating!!!\n");
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
    USLOSS_PsrSet(0x06);
}

void kernelGetPid(USLOSS_Sysargs* args) {
    args->arg1 = getpid();
    USLOSS_PsrSet(0x06);
}
