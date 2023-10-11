/**
 * File: phase2.c
 * Authors: David McLain, Miles Gendreau
 *
 * Purpose: phase2 enables IPC by implementing mailboxes.
 * Supports mailboxes of both zero and nonzero size, and 
 * implements interrupt handlers for disk and terminal.
 * Handles sending and receiving messages by either directly
 * sending them to the process waiting or enqueueing messages
 * if nothing is waiting to receive yet, along with setting up
 * basis for syscalls. For now all syscalls are unimplemented 
 * and call nullsys.
 */
#include <phase1.h>
#include <phase2.h>
#include <stdio.h>
#include <string.h>
#include <usloss.h>

#define CLOCK_INDEX 0
#define TERM_INDEX USLOSS_CLOCK_UNITS
#define DISK_INDEX TERM_INDEX + USLOSS_CLOCK_UNITS

#define AWAITING_DEVICE 1
#define INVALID_SEND -1
#define MAX_SLOTS_PASSED -2
#define WAIT_RECV 20
#define WAIT_SEND 21

/* ---------- Data Structures ----------*/

typedef struct PCB {
    char awaitingDevice;
    char hasMessage;
    char sentMessage;
    char message[MAX_MESSAGE];

    int pid;
    int size;

    struct PCB* nextConsumer;
    struct PCB* nextProducer;
} PCB;

typedef struct Message {
    char inUse;
    char message[MAX_MESSAGE];
    
    int size;

    struct Message* nextSlot;
} Message;

typedef struct Mailbox {
    int id;
    int slotSize;
    int slots;
    int slotsInUse;

    char isReleased;
    char inUse;

    Message* messageHead;
    Message* messageTail;

    PCB* consumerHead;
    PCB* consumerTail;

    PCB* producerHead;
    PCB* producerTail;
} Mailbox;

/* ---------- Globals ---------- */

Mailbox mailboxes[MAXMBOX];     // all available mailboxes for IPC
Message messageSlots[MAXSLOTS]; // all available message slots for all mailboxes
PCB processes[MAXPROC];         // phantom process table, useful for queues

int mboxID = 0;             // id of current open mailbox
int prevClockMsgTime = 0;   // last time a message was sent to the clock mailbox
int slotsInUse = 0;         // counter for how many message slots are being used

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);


/* ---------- Prototypes ----------*/

int disableInterrupts();
int recvMessage(Mailbox*, char*, Message*);
int validateSend(int, void*, int);
int zeroSlotHelper(Mailbox*, char, char);

Message* nextOpenSlot();

void addToQueue(Mailbox*, char);
void checkMode(char*);
void diskAndTermHandler(int, void*);
void nullsys(USLOSS_Sysargs*);
void printMailboxes();
void putInMailbox(Mailbox*, Message*);
void restoreInterrupts(int);
void sendMessage(Mailbox*, char*, int);
void setMboxID();
void syscallHandler(int, void*);

/* ---------- Phase 2 Functions ----------*/

/**
 * Purpose:
 * Initializes data structures and interrupt vector for phase 2
 * 
 * Parameters:
 * None
 *
 * Return:
 * None
 */ 
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
void phase2_start_service_processes() {} // no-op, don't need any service procs here

/**
 * Purpose:
 * Creates a new mailbox with the lowest possible mailbox id with a 
 * specified amount of slots, and specified size in bytes for each slot
 * 
 * Parameters:
 * int slots        the amount of slots the mailbox will use
 * int slot_size    the size, in bytes, each slot will use
 *
 * Return:
 * int  id of the mailbox created
 */ 
int MboxCreate(int slots, int slot_size) {
    checkMode("MboxCreate");
    int prevInt = disableInterrupts();

    if (slots < 0 || slot_size < 0 || slots > MAXSLOTS || mailboxes[mboxID].inUse || slot_size > MAX_MESSAGE) {
        restoreInterrupts(prevInt);
        return -1;
    }
    Mailbox* cur = &mailboxes[mboxID];
    cur->id = mboxID;
    cur->slots = slots;
    cur->slotSize = slot_size;
    cur->inUse = 1;

    // move mboxID to next available index in mailboxes array
    setMboxID();
    restoreInterrupts(prevInt);
    return cur->id;
}

/**
 * Purpose:
 * Releases a mailbox and unblocks all processes that were waiting to
 * send/recv a message through that mailbox
 * 
 * Parameters:
 * mbox_id  id of mailbox to release
 *
 * Return:
 * int  if release is successful 0, otherwise 1
 */ 
int MboxRelease(int mbox_id) {
    checkMode("MboxRelease");
    int prevInt = disableInterrupts();

    if (!mailboxes[mbox_id].inUse || mailboxes[mbox_id].isReleased) {
        restoreInterrupts(prevInt);
        return -1;
    }
    Mailbox* mbox = &mailboxes[mbox_id];
    mbox->isReleased = 1;
    if (!mbox->consumerHead && !mbox->producerHead) {
        memset(mbox, 0, sizeof(Mailbox));
        setMboxID();
    }
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

    restoreInterrupts(prevInt);
    return 0;
}

/**
 * Purpose:
 * Sends a message through a mailbox. Message may be delivered directly to
 * consumer or queued in mail slot. This function may block if there are no
 * consumers or space available to queue a message.
 * 
 * Parameters:
 * int mbox_id      id of mailbox to send message to
 * void* msg_ptr    message to send to mailbox
 * int msg_size     size of message being sent
 *
 * Return:
 * int  if successful 0, else returns value associated with different errors
 */ 
int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {
    checkMode("MboxSend");
    int prevInt = disableInterrupts();

    // validate arguments for send
    int invalid = validateSend(mbox_id, msg_ptr, msg_size);
    if (invalid) {
        restoreInterrupts(prevInt);
        return invalid;
    }

    Mailbox* curMbox = &mailboxes[mbox_id];

    // handle zero slot mailbox
    if (!curMbox->slots) {
        int ret = zeroSlotHelper(curMbox, 1, 0);
        restoreInterrupts(prevInt);
        return ret;
    }

    // add process to queue if queue is full
    if (curMbox->slotsInUse == curMbox->slots && curMbox->slots) {
        PCB* temp = &processes[getpid() % MAXPROC];
        memcpy(temp->message, msg_ptr, msg_size);
        temp->size = msg_size;
        addToQueue(curMbox, 0);
        blockMe(WAIT_RECV);
    }

    // if mailbox released while blocked
    if (curMbox->isReleased) {
        restoreInterrupts(prevInt);
        return -3;
    }
    
    // if message was sent while blocked
    PCB* temp = &processes[getpid() % MAXPROC];
    if (temp->sentMessage) { 
        temp->sentMessage = 0;
        restoreInterrupts(prevInt);
        return 0;
    }

    // send message as normal
    sendMessage(curMbox, msg_ptr, msg_size);

    restoreInterrupts(prevInt);
    return 0;
}

/**
 * Purpose:
 * Receives a message through a mailbox. If message is already available 
 * will read the message and return, otherwise blocks until there is a message
 * available.
 * 
 * Parameters:
 * int mbox_id          id of mailbox to receive message from
 * void* msg_ptr        out pointer to store message received
 * int msg_max_size     max size message is allowed to receive
 *
 * Return:
 * int  if successful returns size of message, else specific error code
 */ 
int MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    checkMode("MboxRecv");
    int prevInt = disableInterrupts();

    // validate arguments for recv
    if (mbox_id < 0 || mbox_id >= MAXMBOX) {
        restoreInterrupts(prevInt);
        return -1;
    }

    Mailbox* curMbox = &mailboxes[mbox_id];
    if (curMbox->isReleased) {
        restoreInterrupts(prevInt);
        return -1;
    }

    // handle zero slot mailbox
    if (!curMbox->slots) {
        restoreInterrupts(prevInt);
        return zeroSlotHelper(curMbox, 0, 0);
    }

    // handle if no messages are available to recv
    Message* msg = curMbox->messageHead;
    if (msg == NULL) {
        addToQueue(curMbox, 1);
        blockMe(WAIT_SEND);
    }

    // if mailbox was released while blocked
    if (curMbox->isReleased) {
        restoreInterrupts(prevInt);
        return -3;
    }

    // if message was recv'd directly while blocked
    PCB* cur = &processes[getpid() % MAXPROC];
    if (cur->hasMessage) {
        if (cur->size > msg_max_size) {
            restoreInterrupts(prevInt);
            return -1;
        }
        cur->hasMessage = 0;

        memcpy(msg_ptr, cur->message, msg_max_size);
        restoreInterrupts(prevInt);
        return cur->size;
    }

    if (msg->size > msg_max_size) {
        restoreInterrupts(prevInt);
        return -1;
    }

    // recv as normal
    int ret = recvMessage(curMbox, msg_ptr, msg);

    restoreInterrupts(prevInt);
    return ret;
}

/**
 * Purpose:
 * Sends a message through a mailbox. Message may be delivered directly to
 * consumer or queued in mail slot. This function may not block, instead if
 * there are no consumers available and no slots to queue message the function
 * returns an error value
 * 
 * Parameters:
 * int mbox_id      id of mailbox to send message to
 * void* msg_ptr    message to send to mailbox
 * int msg_size     size of message being sent
 *
 * Return:
 * int  if successful 0, else returns value associated with different errors
 */ 
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {
    checkMode("MboxCondSend");
    int prevInt = disableInterrupts();
    int invalid = validateSend(mbox_id, msg_ptr, msg_size);
    if (invalid) {
        restoreInterrupts(prevInt);
        return invalid;
    }

    Mailbox* curMbox = &mailboxes[mbox_id];

    if (!curMbox->slots) {
        int ret = zeroSlotHelper(curMbox, 1, 1);
        restoreInterrupts(prevInt);
        return ret;
    }

    if (curMbox->slotsInUse == curMbox->slots) {
        restoreInterrupts(prevInt);
        return -2;
    }

    sendMessage(curMbox, msg_ptr, msg_size);

    restoreInterrupts(prevInt);
    return 0;
}

/**
 * Purpose:
 * Receives a message through a mailbox. If message is already available 
 * will read the message and return, otherwise returns error value
 * 
 * Parameters:
 * int mbox_id          id of mailbox to receive message from
 * void* msg_ptr        out pointer to store message received
 * int msg_max_size     max size message is allowed to receive
 *
 * Return:
 * int  if successful returns size of message, else specific error code
 */ 
int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    checkMode("MboxCondRecv");
    int prevInt = disableInterrupts();

    Mailbox* curMbox = &mailboxes[mbox_id];

    if (curMbox->isReleased) {
        restoreInterrupts(prevInt);
        return -1;
    }

    if (!curMbox->slots) {
        restoreInterrupts(prevInt);
        return zeroSlotHelper(curMbox, 0, 1);
    }

    Message* msg = curMbox->messageHead;
    if (!msg) {
        restoreInterrupts(prevInt);
        return -2;
    }
    
    if (msg->size > msg_max_size) {
        restoreInterrupts(prevInt);
        return -1;
    }

    int ret = recvMessage(curMbox, msg_ptr, msg);

    restoreInterrupts(prevInt);
    return ret;
}

/**
 * Purpose:
 * Waits for an interrupt to fire on a specific device
 * 
 * Parameters:
 * int type     type of device we are waiting for
 * int unit     which unit of device we are waiting for
 * int* status  out parameter to store status in
 *
 * Return:
 * None
 */ 
void waitDevice(int type, int unit, int *status) {
    checkMode("waitDevice");
    int prevInt = disableInterrupts();
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

    restoreInterrupts(prevInt);
}

/**
 * Purpose:
 * Checks to see if any processes are awaiting a device if 
 * all processes are blocked
 * 
 * Parameters:
 * None
 *
 * Return:
 * int  if no process is awaiting a device 0, else AWAITING_DEVICE
 */ 
int phase2_check_io() {
    checkMode("phase2_check_io");
    int prevInt = disableInterrupts();

    for (int i = 0; i < MAXPROC; i++) {
        if (processes[i].awaitingDevice) {
            restoreInterrupts(prevInt);
            return AWAITING_DEVICE;
        }
    }

    restoreInterrupts(prevInt);
    return 0;
}

/**
 * Purpose:
 * Sends a message to the mailbox for clock interrupts if more than 
 * 100 ms have passed
 * 
 * Parameters:
 * None
 *
 * Return:
 * None
 */ 
void phase2_clockHandler() {
    // only send a new message if 100 ms have passed since the last was sent
    int curTime = currentTime();
    if (curTime < prevClockMsgTime + 100000) { return; }

    MboxCondSend(CLOCK_INDEX, &curTime, sizeof(int));
    prevClockMsgTime = curTime;
}

/**
 * Purpose:
 * Handles unimplemented syscalls
 * 
 * Parameters:
 * USLOSS_Sysargs* args     args for syscall called
 *
 * Return:
 * None
 */ 
void nullsys(USLOSS_Sysargs* args) {
    USLOSS_Console("nullsys(): Program called an unimplemented syscall.  syscall no: %d   PSR: 0x%02x\n", args->number, USLOSS_PsrGet());
    USLOSS_Halt(1);
}

/**
 * Not implemented in our version of phase 2
 */
void wakeupByDevice(int type, int unit, int status) {}

    /* ---------- Helper Functions ---------- */

/**
 * Purpose:
 * Helper function for sending and receiving with a zero slot mailbox
 * 
 * Parameters:
 * Mailbox* curMbox     current mailbox we are handling
 * char isSend          whether we are sending to the mailbox or receiving (0 or 1)
 * char isCond          whether or not we are doing a conditional send/recv (0 or 1)
 *
 * Return:
 * int  return status of send/recv operation
 */ 
int zeroSlotHelper(Mailbox* curMbox, char isSend, char isCond) {
    // handle if we are performing a send on zero slot mailbox
    if (isSend) {
        if (curMbox->consumerHead) {
            PCB* proc = curMbox->consumerHead;
            curMbox->consumerHead = curMbox->consumerHead->nextConsumer;
            unblockProc(proc->pid);
        }
        else {
            if (isCond) { return -2; }
            addToQueue(curMbox, 0);
            blockMe(WAIT_RECV);
        }
        return curMbox->isReleased ? -3 : 0;
    }
    // handle if we are performing a recv on zero slot mailbox
    else {
        if (curMbox->producerHead) {
            PCB* proc = curMbox->producerHead;
            curMbox->producerHead = curMbox->producerHead->nextProducer;
            unblockProc(proc->pid);
        }
        else {
            if (isCond) { return -2; }
            addToQueue(curMbox, 1);
            blockMe(WAIT_SEND);
        }
        return curMbox->isReleased ? -3 : 0;
    }
}

/**
 * Purpose:
 * Sets mboxID to be the mailbox slot with the lowest value 
 * that is not in use
 * 
 * Parameters:
 * None
 *
 * Return:
 * None
 */ 
void setMboxID() {
    Mailbox* temp = &mailboxes[mboxID];
    for (int i = 0; i < MAXMBOX; i++) {
        temp = &mailboxes[i];
        if (!temp->inUse) { 
            mboxID = i;
            break;
        }
    }
}

/**
 * Purpose:
 * Handles interrupt from disk and terminal
 * 
 * Parameters:
 * int intType      type of interrupt received  
 * void* payload    payload of interrupt (?)
 *
 * Return:
 * None
 */ 
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
    MboxCondSend(devMboxID + unit, &status, sizeof(int));
}

/**
 * Purpose:
 * Helper function to actually send a message either directly to
 * a process, or queue it into the mailbox if there is no process
 * in the consumer queue
 * 
 * Parameters:
 * Mailbox* curMbox     mailbox we are sending a message to
 * char* msg_ptr        message we are sending to the mailbox
 * int msg_size         size of message we are sending
 *
 * Return:
 * None
 */ 
void sendMessage(Mailbox* curMbox, char* msg_ptr, int msg_size) {
    // directly deliver message to consumer waiting if there is one
    if (curMbox->consumerHead) {
        PCB* temp = curMbox->consumerHead;
        temp->size = msg_size;
        memcpy(temp->message, msg_ptr, msg_size);
        temp->hasMessage = 1;
        curMbox->consumerHead = curMbox->consumerHead->nextConsumer;
        unblockProc(temp->pid);
        return;
    }
    // queue message into slot if no consumer is waiting
    Message* msg = nextOpenSlot();
    memcpy(msg->message, msg_ptr, msg_size);
    msg->size = msg_size;
    msg->inUse = 1;
    putInMailbox(curMbox, msg);
    curMbox->slotsInUse++;
    slotsInUse++;
}

/**
 * Purpose:
 * Helper function to actually receive a message from a mailbox,
 * if there is a process in the consumer queue, manually adds to
 * message queue in order to have messages receive and send in
 * proper order
 * 
 * Parameters:
 * Mailbox* curMbox     mailbox we are receiving a message from
 * char* msg_ptr        out pointer for message to receive
 * Message* msg         current message to read
 *
 * Return:
 * int  length of message received
 */ 
int recvMessage(Mailbox* curMbox, char* msg_ptr, Message* msg) {
    // recv a message through queue
    curMbox->messageHead = curMbox->messageHead->nextSlot;
    memcpy(msg_ptr, msg->message, msg->size);
    int ret = msg->size;
    memset(msg, 0, sizeof(Message));
    curMbox->slotsInUse--;
    slotsInUse--;

    // directly enqueue messages from senders if space opens
    if (curMbox->producerHead) {
        PCB* toUnblock = curMbox->producerHead;
        curMbox->producerHead = curMbox->producerHead->nextProducer;
        Message* msg = nextOpenSlot();
        msg->size = toUnblock->size;
        msg->inUse = 1;
        memcpy(msg->message, toUnblock->message, toUnblock->size);
        putInMailbox(curMbox, msg);
        toUnblock->sentMessage = 1;
        curMbox->slotsInUse++;
        slotsInUse++;
        unblockProc(toUnblock->pid);
    }
    return ret;
}

/**
 * Purpose:
 * Validates information passed into send
 * 
 * Parameters:
 * int id       id of mailbox to send to
 * void* msg    message being sent to the mailbox
 * int size     size of message being sent
 *
 * Return:
 * int  if arguments are valid 0, else error code associated with issue
 */ 
int validateSend(int id, void* msg, int size) {
    if (mailboxes[id].isReleased || id >= MAXMBOX || !mailboxes[id].inUse ||
        size > mailboxes[id].slotSize) {
        return INVALID_SEND;
    }
    if (slotsInUse >= MAXSLOTS) {
        return MAX_SLOTS_PASSED;
    }
    return 0;
}

/**
 * Purpose:
 * Finds next open message slot and returns pointer to it
 * 
 * Parameters:
 * None
 *
 * Return:
 * Message*     pointer to next message slot that is available
 */ 
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
 * Puts a message into a mailbox
 * 
 * Parameters:
 * Mailbox* mbox    mailbox to put message in
 * Message* msg     message to put in mailbox
 *
 * Return:
 * None
 */ 
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

/**
 * Purpose:
 * Adds a process to either consumer or producer queue, depending 
 * on if it is sending or receiving the message
 * 
 * Parameters:
 * Mailbox* mbox    mailbox we want process to enter queues for
 * char isConsumer  whether process we want to add to queue is consumer or not
 *
 * Return:
 * None
 */ 
void addToQueue(Mailbox* mbox, char isConsumer) {
    PCB* proc = &processes[getpid() % MAXPROC];
    proc->pid = getpid();
    // handle if we are adding to consumer queue
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
    // handler if we are adding to producer queue
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
void syscallHandler(int dev, void* arg) {
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
