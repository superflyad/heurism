#ifndef COMPANION_VM_MANAGEMENT_H
#define COMPANION_VM_MANAGEMENT_H
#include "network.h"
int vm_management_init(Network *);
void vm_management_schedule_reboot(void);
void vm_management_poll(int);
#endif
