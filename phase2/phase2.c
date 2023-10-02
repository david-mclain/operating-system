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
    char isUsed;
    char message[MAX_MESSAGE];
    struct Message* nextSlot;
    struct Message* prevSlot;
} Message;

typedef struct Mailbox {
    int id;
    int slots;
    int slotSize;

    char isUsed;

    Message* firstSlot;
} Mailbox;

static Mailbox mailboxes[MAXMBOX];
int mboxID = 0;

Message messageSlots[MAXSLOTS];

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);
void nullsys(void);

void phase2_init(void) {
    memset(mailboxes, 0, sizeof(mailboxes));
    memset(messageSlots, 0, sizeof(messageSlots));
    for (int i = 0; i < MAXSYSCALLS; i++) {
        systemCallVec[i] = &nullsys;
    }
    for (int i = 0; i < USLOSS_CLOCK_UNITS + USLOSS_DISK_UNITS + USLOSS_TERM_UNITS; i++) {
        MboxCreate(1, sizeof(int));
    }
}

void phase2_start_service_processes() {

}

int MboxCreate(int slots, int slot_size) {
    if (slots < 0 || slot_size < 0 || slot_size > MAXSLOTS || mailboxes[mboxID].isUsed) {
        return -1;
    }
    Mailbox* cur = &mailboxes[mboxID];
    cur->id = mboxID;
    cur->slots = slots;
    cur->slotSize = slot_size;
    cur->isUsed = 1;

    // move mboxID to next available index in mailboxes array
    Mailbox* temp = &mailboxes[mboxID];
    for (int i = 0; i < MAXMBOX; i++) {
        mboxID = (mboxID + 1) % MAXMBOX;
        temp = &mailboxes[mboxID];
        if (!temp->isUsed) { break; }
    }

    return cur->id;
}

int MboxRelease(int mbox_id) {
    return 0;
}

int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {
    return 0;
}

int MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    return 0;
}

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {
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

void nullsys() {
    USLOSS_Console("nullsys(): Program called an unimplemented syscall.  syscall no: %d   PSR: 0x%X", 0, USLOSS_PsrGet()); //fill with proper vals later 
}
