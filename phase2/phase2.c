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

    char isReleased;

    Message* firstSlot;
} Mailbox;

Message messageSlots[MAXSLOTS];

int curMailboxID = 0;

static Mailbox mailboxes[MAXMBOX];

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
    if (slots < 0 || slot_size < 0 || slot_size > MAXSLOTS) {
        return -1;
    }
    Mailbox* cur = &mailboxes[curMailboxID];
    cur->id = curMailboxID++;
    cur->slots = slots;
    cur->slotSize = slot_size;
    for (int i = 0; i < slots; i++) {
        int j = i;
        Message* slot = &messageSlots[j];
        while (slot->isUsed) {
            slot = &messageSlots[++j];
            if (j >= MAXSLOTS) {
                printf("erororroror\n");
                return -1;
                //USLOSS_Console("errororororo\n");
                //USLOSS_Halt(1);
            }
        }
        slot->nextSlot = cur->firstSlot;
        if (cur->firstSlot) {
            cur->firstSlot->prevSlot = slot;
        }
        cur->firstSlot = slot;
        slot->isUsed = 1;
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
