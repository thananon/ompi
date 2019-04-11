#include "opal/class/opal_free_list.h"

#include "ompi/mpiext/mpiext.h"
#include "mpiext_sync_types.h"

ompi_mpix_sync_component_t ompi_mpix_sync_component = {0};

static void sync_completion_construct (ompi_mpix_sync_completion_object_t *c_obj)
{
    c_obj->cbdata = NULL;
    c_obj->request = NULL;
}


OBJ_CLASS_INSTANCE( ompi_mpix_sync_completion_object_t,
                    opal_free_list_item_t,
                    sync_completion_construct,
                    NULL);

static int sync_init(void)
{
    OBJ_CONSTRUCT(&ompi_mpix_sync_component.completion_pool, opal_free_list_t);
    opal_free_list_init (&ompi_mpix_sync_component.completion_pool,
                            sizeof(ompi_mpix_sync_completion_object_t),
                            opal_cache_line_size,
                            OBJ_CLASS(ompi_mpix_sync_completion_object_t),
                            0,
                            opal_cache_line_size,
                            512,
                            -1,
                            512,
                            NULL, 0, NULL, NULL, NULL);

    return OPAL_SUCCESS;
}

static int sync_finalize(void)
{
    OBJ_DESTRUCT(&ompi_mpix_sync_component.completion_pool);
    return OPAL_SUCCESS;
}

ompi_mpiext_component_t ompi_mpiext_sync = {
    sync_init,
    sync_finalize
};
