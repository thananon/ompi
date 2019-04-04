#include "ompi_config.h"
#include "ompi/request/request.h"

#include "ompi/mpiext/sync/c/mpiext_sync_c.h.in"

typedef struct ompi_mpix_sync_s
{
    /* inherited ompi_wait_sync_t */
    ompi_wait_sync_t super;

    /* additional properties */
    int32_t num_completed;
    opal_list_t completed_list;
} ompi_mpix_sync_t;


int MPIX_Sync_init(MPIX_Sync *sync)
{

    printf("sync is at %p pointing to %p\n",sync,*sync);
    ompi_mpix_sync_t *tmp;
    tmp = calloc(1,sizeof(ompi_mpix_sync_t));

    tmp->num_completed = 0;
    OBJ_CONSTRUCT(&tmp->completed_list, opal_list_t);
    printf("sync initialized at %p\n", tmp);
    *sync = tmp;
    printf("sync is pointing to at %p\n", *sync);
    return OPAL_SUCCESS;
}

void MPIX_Sync_free(MPIX_Sync sync)
{
    OBJ_DESTRUCT(&sync->completed_list);
    free(sync);
    sync = NULL;
}

int MPIX_Sync_size(MPIX_Sync sync)
{
    return sync->super.count;
}

int MPIX_Sync_query_completed(MPIX_Sync sync)
{
    return sync->num_completed;
}

int MPIX_Sync_attach(MPIX_Sync sync, MPI_Request *requests, int total)
{
    ompi_request_t **reqs = (ompi_request_t**)requests;
    printf("Sync_attach:: sync = %p\n", sync);
    printf("Sync_attach:: requests = %p\n", requests);

    for (int i = 0; i < total; i++) {
        void *tmp_ptr = REQUEST_PENDING;
        ompi_request_t *req = reqs[i];

        /* Attach request to sync, if already completed, skip. */
        if( !OPAL_ATOMIC_COMPARE_EXCHANGE_STRONG_PTR(&req->req_complete, &tmp_ptr, sync) ) {
            total--;
            sync->num_completed++;
            opal_list_append(&sync->completed_list, (opal_list_item_t*)reqs);
            continue;
        }
        printf("Sync_attach::request %p attached to sync %p\n", req, sync);
    }
    OPAL_THREAD_ADD_FETCH32(&sync->super.count, total);
    printf("Sync_attach:: count = %d\n",sync->super.count);
    return total;
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

MPI_Request* MPIX_Sync_get_completed(MPIX_Sync sync)
{
    if (sync->num_completed == 0)
        return NULL;

    return (MPI_Request*) opal_list_remove_first(&sync->completed_list);
}
