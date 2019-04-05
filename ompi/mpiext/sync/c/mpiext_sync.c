#include "ompi/request/request.h"

#include "ompi/mpiext/mpiext.h"
#include "ompi/mpiext/sync/c/mpiext_sync_c.h"
#include "ompi/mpiext/sync/c/mpiext_sync_types.h"

#define QUERY_PROGRESS_THRESHOLD    30

char mpix_sync_empty = 0;

int MPIX_Sync_init(MPIX_Sync *sync)
{
    printf("sync is at %p pointing to %p\n",sync,*sync);
    ompi_mpix_sync_t *tmp;
    tmp = calloc(1,sizeof(ompi_mpix_sync_t));

    tmp->num_completed = 0;
    tmp->state = SYNC_STATE_CLEAN;

    OBJ_CONSTRUCT(&tmp->completion_list, opal_list_t);
    printf("sync initialized at %p\n", tmp);
    *sync = tmp;
    printf("sync is pointing to at %p\n", *sync);
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
    printf("Sync_attach:: sync = %p\n", sync);
    printf("Sync_attach:: request = %p\n", request);
    printf("Sync_attach:: cbdata = %p\n", completion_data);

    ompi_request_t *req = (ompi_request_t*) request;
    req->usr_cbdata = completion_data;

    /* Attach request to sync, if already completed, skip. */
    void *tmp_ptr = REQUEST_PENDING;
    if( !OPAL_ATOMIC_COMPARE_EXCHANGE_STRONG_PTR(&req->req_complete, &tmp_ptr, sync) ) {
        if (req->req_complete == REQUEST_COMPLETED) {
            sync->num_completed++;
            opal_list_append(&sync->completion_list, (opal_list_item_t*)req);
        }
    }

    OPAL_THREAD_ADD_FETCH32(&sync->super.count, 1);
    printf("Sync_attach::request %p attached to sync %p\n", req, sync);
    printf("Sync_attach:: count = %d\n",sync->super.count);

    return OPAL_SUCCESS;
}

int MPIX_Sync_waitall(MPIX_Sync sync)
{
    printf("Sync_waitall:: sync = %p count: %d\n",sync, sync->super.count);
    SYNC_WAIT((ompi_wait_sync_t*)sync);
    return OPAL_SUCCESS;
}

void MPIX_Progress(void)
{
    opal_progress();
}

void* MPIX_Sync_query(MPIX_Sync sync, MPI_Status *status)
{
    static uint16_t visited = 0;
    ompi_request_t *ompi_request;
    void *ret = NULL;

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

    /* get the cbdata to return to the user. */
    ompi_request = c_obj->request;
    printf("c_obj:%p request:%p\n",c_obj,c_obj->request);
    printf("status:src: %d tag:%d\n",
                                         ompi_request->req_status.MPI_SOURCE,
                                         ompi_request->req_status.MPI_TAG);
    *status = ompi_request->req_status;

    ret = c_obj->cbdata;

    /* return the object to the pool */
    MPIX_SYNC_COMPLETION_OBJECT_RETURN(c_obj);

    /* Progressing everytime might be costly, we progress
     * after some threshold. */
    visited++;
    if (visited > QUERY_PROGRESS_THRESHOLD) {
        visited = 0;
        opal_progress();
    }

    return ret;
}
