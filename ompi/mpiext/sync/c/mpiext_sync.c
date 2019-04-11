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

    WAIT_SYNC_INIT((ompi_wait_sync_t*) tmp, 0);
    /* mark this sync as an extension */
    tmp->super.sync_extension = true;
    OBJ_CONSTRUCT(&tmp->super.completed_requests, opal_list_t);

    *sync = tmp;
    return OPAL_SUCCESS;
}

void MPIX_Sync_free(MPIX_Sync *sync)
{
    OBJ_DESTRUCT(&(*sync)->super.completed_requests);
    free(sync);
    *sync = NULL;
}

int MPIX_Sync_size(MPIX_Sync sync)
{
    return sync->super.count;
}

int MPIX_Sync_probe(MPIX_Sync sync)
{
    opal_progress();
    return opal_list_get_size(&sync->super.completed_requests);
}

int MPIX_Sync_attach(MPIX_Sync sync, MPI_Request *request, void *completion_data)
{
    void *tmp_ptr = REQUEST_PENDING;
    ompi_request_t *req = (ompi_request_t*) *request;

    /** printf("Attach: req:%p cbdata:%p\n", *request, completion_data); */

    req->usr_cbdata = completion_data;

    /* Attach request to sync, if already completed, generate completion. */
    if( !OPAL_ATOMIC_COMPARE_EXCHANGE_STRONG_PTR(&req->req_complete, &tmp_ptr, sync) ) {
        if (REQUEST_COMPLETE(req) &&
            MPIX_SYNC_NO_COMPLETION_DATA != completion_data) {
                opal_list_append(&sync->super.completed_requests, (opal_list_item_t*)req);
                goto complete_req;
        } else {
            /* Request already attached itself to another sync */
            /* this should not happen. */
            printf("sync already attached.\n");
            return OPAL_ERR_FATAL;
        }
    }

    /* increase the count if attach success. */
    OPAL_THREAD_ADD_FETCH32(&sync->super.count, 1);

complete_req:
    if ( !req->req_persistent ) {
        *request = MPI_REQUEST_NULL;
    }

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
    int rc = MPI_SUCCESS;
    void *cbdata = NULL;
    static uint16_t visited = 0;

    ompi_request_t *ompi_request;

    /* if queue is empty, try progressing */
    if (opal_list_is_empty(&sync->super.completed_requests)) {
        opal_progress();

        /* if still, nothing completed, return */
        if (opal_list_is_empty(&sync->super.completed_requests))
            return MPIX_SYNC_EMPTY;
    }

    /* retrieve the first completion object. */
    ompi_request = (ompi_request_t*)
                    opal_list_remove_first(&sync->super.completed_requests);

    if ( NULL == ompi_request )
        return MPIX_SYNC_EMPTY;

    cbdata = ompi_request->usr_cbdata;
    /** printf("\t\tquery: request:%p cbdata:%p\n", ompi_request, cbdata); */

    /* Give back the status. */
    if ( MPI_STATUS_IGNORE != status ) {
        *status = ompi_request->req_status;
    }

    /* free the request and catch the error. */
    if ( ompi_request->req_persistent ) {
        ompi_request->req_state = OMPI_REQUEST_INACTIVE;
    } else {
        rc = ompi_request_free(&ompi_request);
        if (rc != MPI_SUCCESS) {
            printf("well..shit\n");
        }
    }

    if ( visited > QUERY_PROGRESS_THRESHOLD ) {
        visited = 0;
        opal_progress();
    }
    visited++;

    return cbdata;
}

int MPIX_Sync_query_bulk(int incount, MPIX_Sync sync, int *outcount, void **cbdata, MPI_Status *status)
{
    int rc = MPI_SUCCESS;
    int err;
    int ncompleted;

    ompi_request_t *ompi_request;

    /* check for completion, if none, progress and check again. */
    ncompleted = opal_list_get_size(&sync->super.completed_requests);
    if (ncompleted == 0) {
        opal_progress();
        ncompleted = opal_list_get_size(&sync->super.completed_requests);
        if (ncompleted == 0) {
            goto bail;
        }
    }

    /* if we have more completions, cap the number with incount;
     * also hint to the user that we have more completions.  */
    if (ncompleted > incount) {
        ncompleted = incount;
        rc = MPIX_SYNC_MORE;
    }

    for (int i=0 ; i < ncompleted ;i++) {
        ompi_request = (ompi_request_t*)
                        opal_list_remove_first(&sync->super.completed_requests);

        /* get the cbdata to return to the user. */
        cbdata[i] = ompi_request->usr_cbdata;

        /* Give back the status. */
        if ( MPI_STATUS_IGNORE != status ) {
            status[i] = ompi_request->req_status;
        }

        /* free the request and catch the error. */
        if ( ompi_request->req_persistent ) {
            ompi_request->req_state = OMPI_REQUEST_INACTIVE;
        } else {
            err = ompi_request_free(&ompi_request);
            if (MPI_SUCCESS != err) {
                return err;
            }
        }
    }
bail:
    *outcount = ncompleted;
    return rc;
}
