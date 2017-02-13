#include "opal/class/opal_task.h"


static void opal_task_construct(opal_task_t*);
static void opal_task_destruct(opal_task_t*);

enum{
    OPAL_TASK_FLAGS_PERSIST = 0x00000001
};

OBJ_CLASS_INSTANCE(
    opal_task_t,
    opal_list_item_t,
    opal_task_construct,
    opal_task_destruct
);

opal_fifo_t *opal_task_queue;

/* Task constructor */
static void opal_task_construct(opal_task_t *task){

    task->func = NULL;
    task->args = NULL;
    task->count = 0;
    task->flags = 0;

}

/* Task destructor */
static void opal_task_destruct(opal_task_t *task){

}


/* Initialize task queue as opal_fifo */

void opal_task_init(){

    /* Create new task queue */
    opal_task_queue = OBJ_NEW(opal_fifo_t);

}

opal_task_t *opal_task_push(opal_task_t *task, opal_fifo_t *queue){
    return (opal_task_t*) opal_fifo_push_atomic(queue,(opal_list_item_t*) task);
}

opal_task_t *opal_task_pop(opal_fifo_t *queue){
    return (opal_task_t*) opal_fifo_pop_atomic(queue);
}


