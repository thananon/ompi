#ifndef OPAL_TASK_H
#define OPAL_TASK_H

#include "opal_config.h"
#include "opal/class/opal_fifo.h"

BEGIN_C_DECLS

struct opal_task_t {
    opal_list_item_t super;
    int32_t flags;
    int32_t count;
    void (*func)(void*);
    void *args;

};

typedef struct opal_task_t opal_task_t;
OPAL_DECLSPEC void opal_task_init(void);
OPAL_DECLSPEC OBJ_CLASS_DECLARATION(opal_task_t);

extern opal_fifo_t opal_task_queue;

void opal_task_init(void);
opal_task_t *opal_task_create(void* funct,void* args);
opal_task_t *opal_task_push(opal_fifo_t *queue, opal_task_t *task);
opal_task_t *opal_task_pop(opal_fifo_t* queue);

END_C_DECLS

#endif /* OPAL_TASK_H */
