#include "ompi/request/request.h"

#include "ompi/mpiext/mpiext.h"
#include "ompi/mpiext/sync/c/mpiext_sync_c.h"
#include "ompi/mpiext/sync/c/mpiext_sync_types.h"

#define QUERY_PROGRESS_THRESHOLD    30

char ompi_mpix_sync_empty = 0;
char ompi_mpix_sync_no_completion_data = 0;

void *MPIX_SYNC_EMPTY               = (void*)&ompi_mpix_sync_empty;
void *MPIX_SYNC_NO_COMPLETION_DATA  = (void*)&ompi_mpix_sync_no_completion_data;


int MPIX_Sync_init(MPIX_Sync *sync)
{
    ompi_mpix_sync_t *tmp;
    tmp = calloc(1,sizeof(ompi_mpix_sync_t));

    tmp->num_completed = 0;
    tmp->state = SYNC_STATE_CLEAN;
    OBJ_CONSTRUCT(&tmp->completion_list, opal_list_t);

    *sync = tmp;
    return OPAL_SUCCESS;
}

void MPIX_Sync_free(MPIX_Sync *sync)
{
    OBJ_DESTRUCT(&((*sync)->completion_list));
    free(*sync);
    *sync = NULL;
}

int MPIX_Sync_size(MPIX_Sync sync)
{
    return sync->super.count;
}

int MPIX_Sync_probe(MPIX_Sync sync)
{
    opal_progress();
    return opal_list_get_size(&sync->completion_list);
}

int MPIX_Sync_attach(MPIX_Sync sync, MPI_Request request, void *completion_data)
{
    void *tmp_ptr = REQUEST_PENDING;
    ompi_request_t *req = (ompi_request_t*) request;

    req->usr_cbdata = completion_data;

    /* Attach request to sync, if already completed, generate completion. */
    if( !OPAL_ATOMIC_COMPARE_EXCHANGE_STRONG_PTR(&req->req_complete, &tmp_ptr, sync) ) {
        if (REQUEST_COMPLETE(req) &&
            MPIX_SYNC_NO_COMPLETION_DATA != completion_data) {
                ompi_mpix_sync_generate_completion(sync, req);
                return OPAL_SUCCESS;
        } else {
            /* Request already attached itself to another sync */
            /* this should not happen. */
            return OPAL_ERR_FATAL;
        }
    }

    /* increase the count if attach success. */
    OPAL_THREAD_ADD_FETCH32(&sync->super.count, 1);
    return OPAL_SUCCESS;
}

int MPIX_Sync_waitall(MPIX_Sync sync)
{
    SYNC_WAIT((ompi_wait_sync_t*)sync);
    return OPAL_SUCCESS;
}

void MPIX_Progress(void)
{
    opal_progress();
}

void* MPIX_Sync_query(MPIX_Sync sync, MPI_Status *status)
{
    int rc;
    void *cbdata = NULL;
    static uint16_t visited = 0;

    ompi_request_t *ompi_request;
    ompi_mpix_sync_completion_object_t *c_obj;

    /* if queue is empty, try progressing */
    if (opal_list_is_empty(&sync->completion_list)) {
        opal_progress();

        /* if still, nothing completed, return */
        if (opal_list_is_empty(&sync->completion_list))
            return MPIX_SYNC_EMPTY;
    }

    /* retrieve the first completion object. */
    c_obj = (ompi_mpix_sync_completion_object_t*)
                opal_list_remove_first(&sync->completion_list);

    if ( NULL == c_obj )
        return MPIX_SYNC_EMPTY;

    /* get the cbdata to return to the user. */
    ompi_request = c_obj->request;
    cbdata = c_obj->cbdata;

    /* Give back the status. */
    if ( MPI_STATUS_IGNORE != status ) {
        *status = ompi_request->req_status;
    }

    /* return the completion object to the pool */
    MPIX_SYNC_COMPLETION_OBJECT_RETURN(c_obj);

    /* free the request and catch the error. */
    rc = ompi_request_free(&ompi_request);
    if (rc != MPI_SUCCESS) {
        printf("well..shit\n");
    }

    if ( visited > QUERY_PROGRESS_THRESHOLD ) {
        visited = 0;
        opal_progress();
    }
    visited++;

    return cbdata;
}
