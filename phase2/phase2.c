#include <phase1.h>
#include <phase2.h>
#include <stdio.h>
#include <string.h>
#include <usloss.h>

#define CLOCK_INDEX 0
#define TERM_INDEX USLOSS_CLOCK_UNITS
#define DISK_INDEX TERM_INDEX + USLOSS_CLOCK_UNITS


/* ---------- Data Structures ----------*/

// MAYBE USE FOR CONSUMER/PRODUCER QUEUES? IDK
typedef struct PCB {
    int pid;    
    struct PCB* nextConsumer;
    struct PCB* nextProducer;
} PCB;

typedef struct Message {
    char inUse;
    char message[MAX_MESSAGE];
    int size;
    struct Message* nextSlot;
    //struct Message* prevSlot;  // I dont think this is needed
} Message;

typedef struct Mailbox {
    int id;
    int slots;
    int slotSize;
    int slotsInUse;

    char inUse;
    char isReleased;

    Message* firstSlot;

    PCB* consumerHead;
    PCB* consumerTail;

    PCB* producerHead;
    PCB* producerTail;
} Mailbox;


/* ---------- Globals ---------- */

PCB processes[MAXPROC];
Mailbox mailboxes[MAXMBOX];
Message messageSlots[MAXSLOTS];

int mboxID = 0;
int slotsInUse = 0;

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);


/* ---------- Prototypes ----------*/

int disableInterrupts();
int validateSend(int, void*, int);

void checkMode(char*);
void restoreInterrupts(int);
void addToConsumer(Mailbox*);
void printQueues();
void removeConsumerHead(Mailbox*);

Message* nextOpenSlot();

static void syscallHandler(int dev, void* arg);
void nullsys(USLOSS_Sysargs*);


/* ---------- Phase 2 Functions ----------*/

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

    int invalid = validateSend(mbox_id, msg_ptr, msg_size);
    if (invalid) { return invalid; }

    Mailbox* curMbox = &mailboxes[mbox_id];
    if (curMbox->slotsInUse == curMbox->slots) {//&& curMbox->consumerHead == NULL) {
        blockMe(20); //idk what val to put here yet
    }
    Message* msg = nextOpenSlot();
    memcpy(msg->message, msg_ptr, msg_size);
    msg->size = msg_size;

    msg->nextSlot = curMbox->firstSlot;
    curMbox->firstSlot = msg;

    if (curMbox->consumerHead) {
        PCB* toUnblock = curMbox->consumerHead;
        removeConsumerHead(curMbox);
        unblockProc(toUnblock->pid);
    }

    restoreInterrupts(prevInt);
    return 0;
}


// just to test my send, no error checking or anything just raw dogging recv
int MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    checkMode("MboxRecv");
    int prevInt = disableInterrupts();

    Mailbox* curMbox = &mailboxes[mbox_id];
    Message* msg = curMbox->firstSlot;
    if (msg == NULL) {
        addToConsumer(curMbox);
        blockMe(20);
    }
    msg = curMbox->firstSlot;
    memcpy(msg_ptr, msg->message, msg_max_size > msg->size ? msg->size : msg_max_size);

    restoreInterrupts(prevInt);
    return msg->size;
}

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {
    checkMode("MboxSend");
    int prevInt = disableInterrupts();
    int valid = validateSend(mbox_id, msg_ptr, msg_size);
    if (!valid) {
        return valid;
    }
    Mailbox* curMbox = &mailboxes[mbox_id];
    if (curMbox->slotsInUse == curMbox->slots) {//&& curMbox->consumerHead == NULL) {
        restoreInterrupts(prevInt);
        return -2;
    }
    Message* msg = nextOpenSlot();
    memcpy(msg->message, msg_ptr, msg_size);
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

void addToConsumer(Mailbox* mbox) {
    PCB* proc = &processes[getpid() % MAXPROC];
    proc->pid = getpid();
    if (!mbox->consumerHead) {
        mbox->consumerHead = proc;
        mbox->consumerTail = proc;
    }
    else {
        mbox->consumerTail->nextConsumer = proc;
        mbox->consumerTail = proc;
    }
}

void removeConsumerHead(Mailbox* mbox) {
    PCB* curHead;
    if (curHead->nextConsumer) {
        mbox->consumerHead = curHead->nextConsumer;
    }
    else {
        mbox->consumerHead = NULL;
        mbox->consumerTail = NULL;
    }
}

void printQueues() {
    Mailbox* mbox;
    for (int i = 0; i < MAXMBOX; i++) {
        mbox = &mailboxes[i];
        if (mbox->inUse) {
            printf("mbox id: %d\n", i);
            PCB* cur = mbox->consumerHead;
            printf("Consumer Queue: ");
            while (cur) {
                printf("%d -> ", cur->pid);
                cur = cur->nextConsumer;
            }
            printf("NULL\n");
            cur = mbox->producerHead;
            printf("Producer Queue: ");
            while (cur) {
                printf("%d -> ", cur->pid);
                cur = cur->nextProducer;
            }
            printf("NULL\n");
        }
    }
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
