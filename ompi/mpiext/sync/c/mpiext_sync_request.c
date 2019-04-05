#include "mpiext_sync_types.h"

int ompi_mpix_sync_generate_completion(ompi_mpix_sync_t *sync, ompi_request_t *req)
{
    /* get completion object */
    ompi_mpix_sync_completion_object_t *c_obj;
    c_obj = (ompi_mpix_sync_completion_object_t*)
                opal_free_list_get (&ompi_mpix_sync_component.completion_pool);

    c_obj->cbdata = req->usr_cbdata;
    c_obj->request = req;
    printf("c_obj generated %p\n",c_obj);

    /* add to the sync's completion list */
    opal_list_append(&sync->completion_list, (opal_list_item_t*) c_obj);
    printf("generate_completion:: req = %p c_obj->request=%p src: %d tag:%d \n", req,
                                                    c_obj->request,
                                                    req->req_status.MPI_SOURCE,
                                                    req->req_status.MPI_TAG);

    return OPAL_SUCCESS;
}
