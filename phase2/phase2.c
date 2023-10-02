#include <phase1.h>
#include <phase2.h>
#include <stdio.h>
#include <string.h>
#include <usloss.h>

typedef struct Mailbox {
    int id;
    char isReleased;
} Mailbox;

static Mailbox mailboxes[MAXMBOX];

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);

void nullsys(void);

void phase2_init(void) {
    memset(mailboxes, 0, sizeof(mailboxes));
    for (int i = 0; i < MAXSYSCALLS; i++) {
        systemCallVec[i] = &nullsys;
    }
    //USLOSS_IntVec[USLOSS_SYSCALL_INT] = 
}

void phase2_start_service_processes() {

}

int MboxCreate(int slots, int slot_size) {
    if (slots < 0 || slot_size < 0 || slot_size > MAXSLOTS) {
        return -1;
    }
    return 0;
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
