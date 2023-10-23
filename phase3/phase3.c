/**
 * File: phase3.c
 * Authors: David McLain, Miles Gendreau
 *
 * Purpose: implement system call handlers to provide the user mode equivalents
 * of fork1(), join(), quit(), readtime(), and currentTime(). Additionally, provide
 * functionality for creating and using semaphores.
 */

#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase3_usermode.h>
#include <string.h>
#include <usloss.h>

#define USER_MODE 0x02

/* ---------- Data Structures ---------- */

typedef struct Process {
    int (*main)(char*);
    char* args;
} Process;

/* ---------- Prototypes ---------- */

void kernelSpawn(USLOSS_Sysargs*);
void kernelWait(USLOSS_Sysargs*);
void kernelTerminate(USLOSS_Sysargs*);
void kernelSemCreate(USLOSS_Sysargs*);
void kernelSemP(USLOSS_Sysargs*);
void kernelSemV(USLOSS_Sysargs*);
void kernelGetTimeOfDay(USLOSS_Sysargs*);
void kernelCPUTime(USLOSS_Sysargs*);
void kernelGetPid(USLOSS_Sysargs*);

int trampoline(char*);

/* ---------- Globals ---------- */

int totalSems = 0;              // num. of semaphores currently allocated
int semaphoreTable[MAXSEMS];    // all available semaphore slots

/* ---------- Phase 3 Functions ---------- */

/**
 * Purpose:
 * Initialize the semaphore table and populate the syscall vector with pointers to
 * each of the syscall handler functions.
 *
 * Parameters:
 * None
 *
 * Return:
 * None
 */
void phase3_init(void) {
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

/**
 * Purpose:
 * None
 *
 * Parameters:
 * None
 *
 * Return:
 * None
 */
void phase3_start_service_processes() {}

/* ---------- Syscall Handlers ---------- */

/**
 * Purpose:
 * Create a new child process by calling fork1().
 *
 * Parameters:
 * USLOSS_Sysargs* args     arguments and out parameters for this system call
 *
 * Return:
 * None
 */
void kernelSpawn(USLOSS_Sysargs* args) {
    int mbox = MboxCreate(1, sizeof(Process));
    
    Process cur;
    cur.main = args->arg1;
    cur.args = args->arg2;
    MboxSend(mbox, (void*)&cur, sizeof(Process));
    
    int pid = fork1((char*)args->arg5, trampoline, (char*)(long)mbox, (int)(long)args->arg3, (int)(long)args->arg4);
    if (pid < 0) {
        args->arg4 = (void*)(long)-1;
        return;
    }

    args->arg1 = (void*)(long)pid;
    args->arg4 = (void*)(long)0;
}

/**
 * Purpose:
 * Bounce the current process into its proper main function.
 *
 * Parameters:
 * char* args   id of mailbox used to recieve this proc's main function and args
 *
 * Return:
 */
int trampoline(char* args) {
    int mbox = (int)(long)(void*)args;
    Process cur;
    MboxRecv(mbox, &cur, sizeof(Process));
    MboxRelease(mbox);

    int (*func)(char*) = cur.main;

    USLOSS_PsrSet(USER_MODE);

    int ret = (*func)(cur.args);
    Terminate(ret);
    return 0; // shouldnt ever get here
}

/**
 * Purpose:
 * Call join() to wait for a child process to die. When one does, store its pid, and
 * exit status in out parameters. If the current process has no children, store -2
 * in another out parameter.
 *
 * Parameters:
 * USLOSS_Sysargs* args     arguments and out parameters for this system call
 *
 * Return:
 * None
 */
void kernelWait(USLOSS_Sysargs* args) {
    int status;
    int pid;
    pid = join(&status);
    args->arg1 = (void*)(long)pid;
    args->arg2 = (void*)(long)status;
    args->arg4 = status == -2 ? (void*)(long)-2 : (void*)(long)0;
}

/**
 * Purpose:
 * Call join() repeatedly until the current process has no remaining children, then
 * call quit().
 *
 * Parameters:
 * USLOSS_Sysargs* args     arguments and out parameters for this system call
 *
 * Return:
 * None
 */
void kernelTerminate(USLOSS_Sysargs* args) {
    int childPid = 1;
    int status;
    while (childPid != -2) {
        childPid = join(&status);
    }
    quit((int)(long)args->arg1);
}

/**
 * Purpose:
 * Create a new semaphore (implemented with a mailbox) and set its value to the
 * initial value given in args. Store the semaphore's id in an out paramter.
 *
 * Parameters:
 * USLOSS_Sysargs* args     arguments and out parameters for this system call
 *
 * Return:
 * None
 */
void kernelSemCreate(USLOSS_Sysargs* args) {
    int initialValue = (int)(long)args->arg1;
    if (totalSems == MAXSEMS || initialValue < 0) {
        args->arg4 = (void*)(long)-1;
        return;
    }

    semaphoreTable[totalSems] = MboxCreate(1, sizeof(int));
    if (initialValue > 0) {
        MboxSend(semaphoreTable[totalSems], &initialValue, sizeof(int));
    }

    args->arg1 = (void*)(long)totalSems++;
    args->arg4 = 0;
}

/**
 * Purpose:
 * Try to decrement the value in the semaphore corresponding to the id given in args.
 * If the value is zero, block to wait for the value to be incremented.
 *
 * Parameters:
 * USLOSS_Sysargs* args     arguments and out parameters for this system call
 *
 * Return:
 * None
 */
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
}

/**
 * Purpose:
 * Increment the value in the semaphore corresponding to the id given in args.
 *
 * Parameters:
 * USLOSS_Sysargs* args     arguments and out parameters for this system call
 *
 * Return:
 * None
 */
void kernelSemV(USLOSS_Sysargs* args) {
    int semId = (int)(long)args->arg1;
    if (!semaphoreTable[semId]) {
        args->arg4 = (void*)(long)-1;
        return;
    }

    int semVal;
    int recvSuccess = MboxCondRecv(semaphoreTable[semId], &semVal, sizeof(int));

    semVal = semVal * (recvSuccess > 0) + 1;
    MboxSend(semaphoreTable[semId], &semVal, sizeof(int));
    
    args->arg4 = (void*)(long)0;
}


/**
 * Purpose:
 * Get the current time of the system in microseconds.
 *
 * Parameters:
 * USLOSS_Sysargs* args     arguments and out parameters for this system call
 *
 * Return:
 * None
 */
void kernelGetTimeOfDay(USLOSS_Sysargs* args) {
    args->arg1 = (void*)(long)currentTime();
}

/**
 * Purpose:
 * Get the total time the current process has taken on the CPU in microseconds.
 *
 * Parameters:
 * USLOSS_Sysargs* args     arguments and out parameters for this system call
 *
 * Return:
 * None
 */
void kernelCPUTime(USLOSS_Sysargs* args) {
    args->arg1 = (void*)(long)readtime();
}

/**
 * Purpose:
 * Get the pid of the current process.
 *
 * Parameters:
 * USLOSS_Sysargs* args     arguments and out parameters for this system call
 *
 * Return:
 * None
 */
void kernelGetPid(USLOSS_Sysargs* args) {
    args->arg1 = (void*)(long)getpid();
}
