/* This file is a header for internal OMPI components.
 * Anything that we expose to the user level must be in
 * mpiext_sync_c.h.in */

#ifndef MPIX_SYNC_H
#define MPIX_SYNC_H

#include "ompi_config.h"
#include "opal/threads/wait_sync.h"
#include "opal/class/opal_free_list.h"
#include "ompi/request/request.h"

extern char ompi_mpix_sync_empty;
extern void *MPIX_SYNC_EMPTY;

#define MPIX_SYNC_COMPLETION_OBJECT_RETURN(a)          \
    opal_free_list_return(                             \
            &ompi_mpix_sync_component.completion_pool, \
            (opal_free_list_item_t*) (a))

enum ompi_mpix_sync_state_enum
{
    SYNC_STATE_CLEAN = 0,
    SYNC_STATE_FIRED,
    SYNC_STATE_ERROR,
    SYNC_STATE_TOTAL
};

typedef struct ompi_mpix_sync_completion_object_s
{
    opal_free_list_item_t super;
    ompi_request_t *request;
    void *cbdata;
} ompi_mpix_sync_completion_object_t;


typedef struct ompi_mpix_sync_s
{
    /* inherited ompi_wait_sync_t */
    ompi_wait_sync_t super;

    /* additional properties */
    int32_t state;
    int32_t num_completed;
    opal_list_t completion_list;

} ompi_mpix_sync_t;

typedef struct ompi_mpix_sync_completion_object_s ompi_mpix_sync_completion_object_t;

typedef struct ompi_mpix_sync_component_s
{
    opal_free_list_t completion_pool;
} ompi_mpix_sync_component_t;

extern ompi_mpix_sync_component_t ompi_mpix_sync_component;

OBJ_CLASS_DECLARATION(ompi_mpix_sync_completion_object_t);

int ompi_mpix_sync_generate_completion(ompi_mpix_sync_t *sync, ompi_request_t *req);
#endif
