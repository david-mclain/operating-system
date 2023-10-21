#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase3_usermode.h>
#include <stdio.h>
#include <string.h>
#include <usloss.h>

#define USER_MODE 0x02

typedef struct PCB {
    int (*main)(char*);
} PCB;

void kernelCPUTime(USLOSS_Sysargs*);
void kernelGetPid(USLOSS_Sysargs*);
void kernelGetTimeOfDay(USLOSS_Sysargs*);
void kernelSemCreate(USLOSS_Sysargs*);
void kernelSemP(USLOSS_Sysargs*);
void kernelSemV(USLOSS_Sysargs*);
void kernelSpawn(USLOSS_Sysargs*);
void kernelTerminate(USLOSS_Sysargs*);
void kernelWait(USLOSS_Sysargs*);

int trampoline(char*);

PCB processes[MAXPROC];
int totalSems = 0;
int semaphoreTable[MAXSEMS];

void phase3_init(void) {
    memset(processes, 0, sizeof(processes));
    memset(semaphoreTable, 0, sizeof(semaphoreTable));

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

void kernelSpawn(USLOSS_Sysargs* args) {
    int len = args->arg2 == NULL ? 0 : strlen((char*)args->arg2) + 1;
    int mbox = MboxCreate(1, len);
    int pid = fork1((char*)args->arg5, trampoline, (char*)(long)mbox, (int)(long)args->arg3, (int)(long)args->arg4);
    if (pid < 0) {
        args->arg4 = -1;
        USLOSS_PsrSet(USER_MODE);
        return;
    }

    PCB* cur = &processes[pid % MAXPROC];
    cur->main = args->arg1;
    args->arg1 = (void*)(long)pid;
    args->arg4 = (void*)(long)0;
    //printf("ereererere\n");
    MboxSend(mbox, args->arg2, len);
}

int trampoline(char* args) {
    int mbox = (int)(long)(void*)args;
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
    args->arg4 = status = -2 ? (void*)(long)-2 : (void*)(long)0;
    USLOSS_PsrSet(USER_MODE);
}

void kernelTerminate(USLOSS_Sysargs* args) {
    int childPid = 1;
    int status;
    while (childPid != -2) {
        childPid = join(&status);
    }
    args->arg2 = (void*)(long)status;
    quit((int)(long)args->arg1);
}

void kernelSemCreate(USLOSS_Sysargs* args) {
    int initialValue = (int)(long)args->arg1;
    if (totalSems == MAXSEMS || initialValue < 0) {
        args->arg4 = (void*)(long)-1;
        USLOSS_PsrSet(USER_MODE);
        return;
    }

    // create mailbox and send initial value
    semaphoreTable[totalSems] = MboxCreate(1, sizeof(int));
    MboxSend(semaphoreTable[totalSems], &initialValue, sizeof(int));

    args->arg1 = (void*)(long)totalSems++;
    args->arg4 = 0;
    USLOSS_PsrSet(USER_MODE);
}

// your mom <3
// shaking n crying rn

void kernelSemP(USLOSS_Sysargs* args) {
    int semId = (int)(long)args->arg1;
    if (!semaphoreTable[semId]) {
        args->arg4 = (void*)(long)-1;
        return;
    }

    int semVal;
    MboxRecv(semaphoreTable[semId], &semVal, sizeof(int));
    if (--semVal > 0) {
        MboxSend(semaphoreTable[semId], &semVal, sizeof(int));
    }

    args->arg4 = (void*)(long)0;
    USLOSS_PsrSet(USER_MODE);
}

void kernelSemV(USLOSS_Sysargs* args) {
    int semId = (int)(long)args->arg1;
    if (!semaphoreTable[semId]) {
        args->arg4 = (void*)(long)-1;
        return;
    }

    int semVal;
    int recvSuccess = MboxCondRecv(semaphoreTable[semId], &semVal, sizeof(int));
    // there could be issues with this. error handling or smth
    // might not have to worry abt it tho
    if (recvSuccess > 0) {
        semVal++;
        MboxSend(semaphoreTable[semId], &semVal, sizeof(int));
    }
    else {
        semVal = 1;
        MboxSend(semaphoreTable[semId], &semVal, sizeof(int));
    }
    
    args->arg4 = (void*)(long)0;
}


void kernelGetTimeOfDay(USLOSS_Sysargs* args) {
    args->arg1 = currentTime();
    USLOSS_PsrSet(USER_MODE);
}

void kernelCPUTime(USLOSS_Sysargs* args) {
    args->arg1 = readtime();
    USLOSS_PsrSet(USER_MODE);
}

void kernelGetPid(USLOSS_Sysargs* args) {
    args->arg1 = getpid();
    USLOSS_PsrSet(USER_MODE);
}
