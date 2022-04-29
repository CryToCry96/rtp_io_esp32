#ifndef DI_DO_H
#define DI_DO_H

//GPIO control task
void gpio_task(void* arg);
//GPIO init task
void io_init();
//Task executed on cpu id
int cpu_num;

//Change DO status
void set_DO();

#endif //DI_DO_H