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
    char awaitingDevice;

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

    Message* messageHead;
    Message* messageTail;

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
int prevClockMsgTime = 0; // last time a message was sent to the clock mailbox

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);


/* ---------- Prototypes ----------*/

int disableInterrupts();
int validateSend(int, void*, int);
int recvMessage(Mailbox*, char*, Message*);

void checkMode(char*);
void restoreInterrupts(int);
void addToQueue(Mailbox*, char);
void putInMailbox(Mailbox*, Message*);
void sendMessage(Mailbox*, char*, int);
void printMailboxes();
void nullsys(USLOSS_Sysargs*);
void diskAndTermHandler(int, void*);

Message* nextOpenSlot();

static void syscallHandler(int, void*);


/* ---------- Phase 2 Functions ----------*/

void phase2_init(void) {
    memset(mailboxes, 0, sizeof(mailboxes));
    memset(messageSlots, 0, sizeof(messageSlots));
    memset(processes, 0, sizeof(processes));
    for (int i = 0; i < MAXSYSCALLS; i++) {
        systemCallVec[i] = &nullsys;
    }
    for (int i = 0; i < USLOSS_CLOCK_UNITS + USLOSS_DISK_UNITS + USLOSS_TERM_UNITS; i++) {
        MboxCreate(1, sizeof(int));
    }
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = &syscallHandler;
    USLOSS_IntVec[USLOSS_DISK_INT] = &diskAndTermHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = &diskAndTermHandler;
}

void phase2_start_service_processes() {} // no-op, don't need any service procs here

int MboxCreate(int slots, int slot_size) {
    checkMode("MboxCreate");
    int prevInt = disableInterrupts();

    if (slots < 0 || slot_size < 0 || slots > MAXSLOTS || mailboxes[mboxID].inUse || slot_size > MAX_MESSAGE) {
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
    Mailbox* mbox = &mailboxes[mbox_id];
    mbox->isReleased = 1;
    PCB* cur = mbox->consumerHead;
    while (cur) {
        unblockProc(cur->pid);
        cur = cur->nextConsumer;
    }
    cur = mbox->producerHead;
    while (cur) {
        unblockProc(cur->pid);
        cur = cur->nextProducer;
    }
    //memset(mbox, 0, sizeof(Mailbox));
    return 0;
}

int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {
    checkMode("MboxSend");
    int prevInt = disableInterrupts();

    int invalid = validateSend(mbox_id, msg_ptr, msg_size);
    if (invalid) { return invalid; }
    Mailbox* curMbox = &mailboxes[mbox_id];
    if (curMbox->isReleased) { return -1; }

    if (curMbox->slotsInUse == curMbox->slots && curMbox->slots != 0) {//&& curMbox->consumerHead == NULL) {
        addToQueue(curMbox, 0);
        blockMe(20); //idk what val to put here yet
    }

    if (curMbox->isReleased) { return -3; }

    sendMessage(curMbox, msg_ptr, msg_size);

    restoreInterrupts(prevInt);
    return 0;
}


int MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    checkMode("MboxRecv");
    int prevInt = disableInterrupts();

    if (mbox_id < 0 || mbox_id >= MAXMBOX) { return -1; } // invalid mailbox

    Mailbox* curMbox = &mailboxes[mbox_id];
    if (curMbox->isReleased) { return -1; } // mailbox is released

    // maybe change this? use varible or smth
    Message* msg = curMbox->messageHead;
    if (msg == NULL) {
        addToQueue(curMbox, 1);
        blockMe(20);
    }
    if (curMbox->isReleased) { return -3; } // mailbox is released
    msg = curMbox->messageHead;
    if (msg->size > msg_max_size) { return -1; } // message is too large

    int ret = recvMessage(curMbox, msg_ptr, msg);

    restoreInterrupts(prevInt);
    return ret;
}

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {
    checkMode("MboxSend");
    int prevInt = disableInterrupts();
    int invalid = validateSend(mbox_id, msg_ptr, msg_size);
    if (invalid) {
        return invalid;
    }
    Mailbox* curMbox = &mailboxes[mbox_id];
    if (curMbox->slotsInUse == curMbox->slots) {//&& curMbox->consumerHead == NULL) {
        restoreInterrupts(prevInt);
        return -2;
    }

    sendMessage(curMbox, msg_ptr, msg_size);

    restoreInterrupts(prevInt);
    return 0;
}

int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    checkMode("MboxRecv");
    int prevInt = disableInterrupts();

    Mailbox* curMbox = &mailboxes[mbox_id];
    Message* msg = curMbox->messageHead;

    // idk why it needs to return -1 here for it to work with test22
    if (curMbox->isReleased) { return -1; } // mailbox is released
    
    if (!msg) { return -2; }

    msg = curMbox->messageHead;
    if (msg->size > msg_max_size) { return -1; } // message is too large

    int ret = recvMessage(curMbox, msg_ptr, msg);

    restoreInterrupts(prevInt);
    return ret;
}

void waitDevice(int type, int unit, int *status) {
    // find the correct mailbox
    int devMboxID = -1; 
    int devUnitErr = 0;
    switch (type) {
        case USLOSS_CLOCK_DEV:
            devMboxID = CLOCK_INDEX;
            if (unit >= USLOSS_CLOCK_UNITS) { devUnitErr = 1; }
            break;
        case USLOSS_TERM_DEV:
            devMboxID = TERM_INDEX;
            if (unit >= USLOSS_TERM_UNITS) { devUnitErr = 1; }
            break;
        case USLOSS_DISK_DEV:
            devMboxID = DISK_INDEX;
            if (unit >= USLOSS_DISK_UNITS) { devUnitErr = 1; }
            break;
        default:
            USLOSS_Console("ERROR: Invalid device type\n");
            USLOSS_Halt(1);
    }
    
    if (devUnitErr) {
        USLOSS_Console("ERROR: Invalid device unit\n");
        USLOSS_Halt(1);
    }

    // set current proc's awaitingDevice flag
    PCB* proc = &processes[getpid() % MAXPROC];
    proc->pid = getpid();
    proc->awaitingDevice = 1;

    // call recv()
    int msg;
    MboxRecv(devMboxID + unit, &msg, sizeof(int));
    proc->awaitingDevice = 0;

    // message is recieved, store it into status
    *status = msg;
}

void wakeupByDevice(int type, int unit, int status) {}

int phase2_check_io() {
    for (int i = 0; i < MAXPROC; i++) {
        if (processes[i].awaitingDevice) {
            return 1; // not sure what to return here... spec just says "nonzero"
        }
    }
    return 0;
}

void phase2_clockHandler() {
    // only send a new message if 100 ms have passed since the last was sent
    int curTime = currentTime();
    if (curTime < prevClockMsgTime + 100000) { return; }

    MboxCondSend(CLOCK_INDEX, &curTime, sizeof(int)); // TODO use condSend
    prevClockMsgTime = curTime;
}

void nullsys(USLOSS_Sysargs* args) {
    USLOSS_Console("nullsys(): Program called an unimplemented syscall.  syscall no: %d   PSR: 0x%02x\n", args->number, USLOSS_PsrGet());
    USLOSS_Halt(1);
}


    /* ---------- Helper Functions ---------- */

void diskAndTermHandler(int intType, void* payload) {
    int devMboxID = -1; 
    switch (intType) {
        case USLOSS_DISK_INT:
            devMboxID = DISK_INDEX;
            break;
        case USLOSS_TERM_INT:
            devMboxID = TERM_INDEX;
            break;
        default:
            USLOSS_Console("ERROR: invalid interrupt type (Shouldn't ever get here)\n");
            USLOSS_Halt(1);
    }
    int unit = (int)(long)payload;

    int status;
    USLOSS_DeviceInput(intType, unit, &status);
    MboxSend(devMboxID + unit, &status, sizeof(int)); // TODO change to condSend
}

void sendMessage(Mailbox* curMbox, char* msg_ptr, int msg_size) {
    Message* msg = nextOpenSlot();
    memcpy(msg->message, msg_ptr, msg_size);
    msg->size = msg_size;
    msg->inUse = 1;
    putInMailbox(curMbox, msg);
    curMbox->slotsInUse++;
    slotsInUse++;

    if (curMbox->consumerHead) {
        PCB* toUnblock = curMbox->consumerHead;
        curMbox->consumerHead = curMbox->consumerHead->nextConsumer;
        unblockProc(toUnblock->pid);
    }
}

int recvMessage(Mailbox* curMbox, char* msg_ptr, Message* msg) {
    curMbox->messageHead = curMbox->messageHead->nextSlot;
    memcpy(msg_ptr, msg->message, msg->size);
    int ret = msg->size;
    memset(msg, 0, sizeof(Message));
    curMbox->slotsInUse--;
    slotsInUse--;

    if (curMbox->producerHead) {
        PCB* toUnblock = curMbox->producerHead;
        curMbox->producerHead = curMbox->producerHead->nextProducer;
        unblockProc(toUnblock->pid);
    }
    return ret;
}

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

void putInMailbox(Mailbox* mbox, Message* msg) {
    if (!mbox->messageHead) {
        mbox->messageHead = msg;
        mbox->messageTail = msg;
    }
    else {
        mbox->messageTail->nextSlot = msg;
        mbox->messageTail = msg;
    }
}

void addToQueue(Mailbox* mbox, char isConsumer) {
    PCB* proc = &processes[getpid() % MAXPROC];
    proc->pid = getpid();
    if (isConsumer) {
        if (!mbox->consumerHead) {
            mbox->consumerHead = proc;
            mbox->consumerTail = proc;
        }
        else {
            mbox->consumerTail->nextConsumer = proc;
            mbox->consumerTail = proc;
        }
    }
    else {
        if (!mbox->producerHead) {
            mbox->producerHead = proc;
            mbox->producerTail = proc;
        }
        else {
            mbox->producerTail->nextProducer = proc;
            mbox->producerTail = proc;
        }
    }
}

void printMailboxes() {
    Mailbox* mbox;
    for (int i = 0; i < MAXMBOX; i++) {
        mbox = &mailboxes[i];
        if (mbox->inUse) {
            printf("mbox id: %d\n", i);
            
            printf("Message List: ");
            Message* curMsg = mbox->messageHead;
            while (curMsg) {
                printf("%s -> ", curMsg->message);
                curMsg = curMsg->nextSlot;
            }
            printf("NULL\n");

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
    USLOSS_Sysargs* args = (USLOSS_Sysargs*)arg;
    if (args->number < 0 || args->number >= MAXSYSCALLS) {
        USLOSS_Console("syscallHandler(): Invalid syscall number %d\n", args->number);
        USLOSS_Halt(1);
    }
    (*systemCallVec)(args);
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
