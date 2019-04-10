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

    /* mark this sync as an extension */
    tmp->super.sync_extension = true;

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
                ompi_mpix_sync_generate_completion(sync, req);
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
    /** printf("\t\tquery: request:%p cbdata:%p\n", ompi_request, cbdata); */

    /* Give back the status. */
    if ( MPI_STATUS_IGNORE != status ) {
        *status = ompi_request->req_status;
    }

    /* return the completion object to the pool */
    MPIX_SYNC_COMPLETION_OBJECT_RETURN(c_obj);

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

void MPIX_Sync_query_bulk(int incount, MPIX_Sync sync, int *outcount, void **cbdata, MPI_Status *status)
{
    int rc = MPI_SUCCESS;
    int nret, ncompleted;

    static uint16_t visited = 0;

    volatile opal_list_item_t *ptr,*sentinel;
    ompi_request_t *ompi_request;
    ompi_mpix_sync_completion_object_t *c_obj;

    ncompleted = opal_list_get_size(&sync->completion_list);
    if (ncompleted != 0) {
        /** printf("query_bulk: asked for %d and found %d completion.\n",incount, ncompleted); */
    } else {
        opal_progress();
        *outcount = 0;
        return;
    }
    nret = ncompleted;
    if (ncompleted > incount) {
        nret = incount;
    }

    for (int i=0;i<nret;i++) {
        c_obj = (ompi_mpix_sync_completion_object_t*)
                    opal_list_remove_first(&sync->completion_list);

        /* get the cbdata to return to the user. */
        ompi_request = c_obj->request;
        cbdata[i] = c_obj->cbdata;

        /* Give back the status. */
        if ( MPI_STATUS_IGNORE != status ) {
            status[i] = ompi_request->req_status;
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
        /* return the completion object to the pool */
        MPIX_SYNC_COMPLETION_OBJECT_RETURN(c_obj);
    }

    *outcount = nret;
    return;
}
