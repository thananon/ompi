#include "opal/class/opal_fifo.h"

typedef struct{

    int32_t flags;
    int32_t count;
    void (*func)(void*);
    void *args;

}opal_task_t;



void opal_task_init(void);
opal_task_t *opal_task_create(void* funct,void* args);
opal_task_t *opal_task_push(opal_task_t *task, opal_fifo_t *queue);
opal_task_t *opal_task_pop(opal_fifo_t* queue);
