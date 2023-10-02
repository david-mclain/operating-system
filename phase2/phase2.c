#include "phase2.h"

int MboxCreate(int slots, int slot_size) {
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

void wakeupByDevice(int type, int unit, int status) {
    
}

void phase2_init(void) {
    
}

void phase2_start_service_processes() {

}

int phase2_check_io() {
    return 0;
}

void phase2_clockHandler() {

}

void (*systemCallVec[])(USLOSS_Sysargs *args);
