#include <phase1.h>
#include <phase2.h>
#include <stdio.h>
#include <string.h>
#include <usloss.h>

#define CLOCK_INDEX 0
#define TERM_INDEX USLOSS_CLOCK_UNITS
#define DISK_INDEX TERM_INDEX + USLOSS_CLOCK_UNITS

typedef struct PCB {
    int pid;    
} PCB;

typedef struct Message {
    char inUse;
    char message[MAX_MESSAGE];
    struct Message* nextSlot;
    struct Message* prevSlot;
} Message;

typedef struct Mailbox {
    int id;
    int slots;
    int slotSize;

    char inUse;
    char isReleased;

    Message* firstSlot;
} Mailbox;

Message* nextOpenSlot();
void checkMode(char*);
void restoreInterrupts(int);
int disableInterrupts();
static void syscallHandler(int dev, void* arg);
int validateSend(int, void*, int);

static Mailbox mailboxes[MAXMBOX];
int mboxID = 0;
int slotsInUse = 0;

Message messageSlots[MAXSLOTS];

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);
void nullsys(USLOSS_Sysargs*);

void phase2_init(void) {
    memset(mailboxes, 0, sizeof(mailboxes));
    memset(messageSlots, 0, sizeof(messageSlots));
    for (int i = 0; i < MAXSYSCALLS; i++) {
        systemCallVec[i] = &nullsys;
    }
    for (int i = 0; i < USLOSS_CLOCK_UNITS + USLOSS_DISK_UNITS + USLOSS_TERM_UNITS; i++) {
        MboxCreate(1, sizeof(int));
    }
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = &syscallHandler;
}

void phase2_start_service_processes() {

}

int MboxCreate(int slots, int slot_size) {
    checkMode("MboxCreate");
    int prevInt = disableInterrupts();
    if (slots < 0 || slot_size < 0 || slots > MAXSLOTS || mailboxes[mboxID].inUse || slot_size >= MAX_MESSAGE) {
        return -1;
    }
    Mailbox* cur = &mailboxes[mboxID];
    cur->id = mboxID;
    cur->slots = slots;
    cur->slotSize = slot_size;
    cur->inUse = 1;

    // move mboxID to next available index in mailboxes array
    Mailbox* temp = &mailboxes[mboxID];
    for (int i = 0; i < MAXMBOX; i++) {
        mboxID = (mboxID + 1) % MAXMBOX;
        temp = &mailboxes[mboxID];
        if (!temp->inUse) { break; }
    }
    restoreInterrupts(prevInt);
    return cur->id;
}

int MboxRelease(int mbox_id) {
    if (!mailboxes[mbox_id].inUse || mailboxes[mbox_id].isReleased) {
        return -1;
    }
    return 0;
}

int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {
    checkMode("MboxSend");
    int prevInt = disableInterrupts();
    int valid = validateSend(mbox_id, msg_ptr, msg_size);
    if (!valid) {
        return valid;
    }
    Message* msg = nextOpenSlot();
    memcpy(msg->message, msg_ptr, msg_size);
    restoreInterrupts(prevInt);
    return 0;
}

int MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    return 0;
}

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {
    checkMode("MboxSend");
    int prevInt = disableInterrupts();
    int valid = validateSend(mbox_id, msg_ptr, msg_size);
    if (!valid) {
        return valid;
    }
    Message* msg = nextOpenSlot();
    restoreInterrupts(prevInt);
    return 0;
}

int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    return 0;
}

void waitDevice(int type, int unit, int *status) {
    
}

void wakeupByDevice(int type, int unit, int status) {}

int phase2_check_io() {
    return 0;
}

void phase2_clockHandler() {

}

void nullsys(USLOSS_Sysargs* args) {
    USLOSS_Console("nullsys(): Program called an unimplemented syscall.  syscall no: %d   PSR: 0x%02x\n", args->number, USLOSS_PsrGet());
    USLOSS_Halt(1);
}

    /* ---------- Helper Functions ---------- */

int validateSend(int id, void* msg, int size) {
    if (mailboxes[id].isReleased) {
        return -3;
    }
    if (slotsInUse >= MAXSLOTS) {
        return -2;
    }
    if (id >= MAXMBOX || !mailboxes[id].inUse || size > mailboxes[id].slotSize) {
        return -1;
    }
    return 0;
}

Message* nextOpenSlot() {
    for (int i = 0; i < MAXSLOTS; i++) {
        if (!messageSlots[i].inUse) {
            return &messageSlots[i];
        }
    }
    return NULL;
}
/**
 * Purpose:
 * Responsible for handling clock interrupts
 * 
 * Parameters:
 * int dev      Device requested from interrupt
 * void* arg    Arguments for device (if needed)
 *
 * Return:
 * None
 */ 
static void syscallHandler(int dev, void* arg) {
    (*systemCallVec)((USLOSS_Sysargs*)arg);
}

/**
 * Purpose:
 * Checks the current mode of the operating system, halts if user mode attempts to
 * execute kernel functions
 * 
 * Parameters:
 * char* fnName     Name of function that user mode process attempted to execute
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
 * int  State that interrupts were in before function call
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
 * int prevInt  State that interrupts were in before function call
 *
 * Return:
 * None
 */ 
void restoreInterrupts(int prevInt) {
    USLOSS_PsrSet(USLOSS_PsrGet() | prevInt);
}
