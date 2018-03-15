/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2014-2018 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "btl_uct.h"
#include "btl_uct_endpoint.h"
#include "opal/util/proc.h"

static void mca_btl_uct_endpoint_construct (mca_btl_uct_endpoint_t *endpoint)
{
    for (int i = 0 ; i < MCA_BTL_UCT_MAX_WORKERS ; ++i) {
        endpoint->uct_eps[i] = NULL;
    }

    OBJ_CONSTRUCT(&endpoint->ep_lock, opal_mutex_t);
}

static void mca_btl_uct_endpoint_destruct (mca_btl_uct_endpoint_t *endpoint)
{
    for (int i = 0 ; i < MCA_BTL_UCT_MAX_WORKERS ; ++i) {
        if (NULL != endpoint->uct_eps[i]) {
            uct_ep_destroy (endpoint->uct_eps[i]);
            endpoint->uct_eps[i] = NULL;
        }
    }

    OBJ_DESTRUCT(&endpoint->ep_lock);
}

OBJ_CLASS_INSTANCE(mca_btl_uct_endpoint_t, opal_list_item_t,
                   mca_btl_uct_endpoint_construct,
                   mca_btl_uct_endpoint_destruct);

mca_btl_base_endpoint_t *mca_btl_uct_endpoint_create (opal_proc_t *proc)
{
    mca_btl_uct_endpoint_t *endpoint = OBJ_NEW(mca_btl_uct_endpoint_t);

    if (OPAL_UNLIKELY(NULL == endpoint)) {
        return NULL;
    }

    endpoint->ep_proc = proc;

    return (mca_btl_base_endpoint_t *) endpoint;
}

int mca_btl_uct_endpoint_connect (mca_btl_uct_module_t *module, mca_btl_uct_endpoint_t *endpoint, int context_id)
{
    uct_device_addr_t *device_addr = NULL;
    uct_iface_addr_t *iface_addr;
    mca_btl_uct_modex_t *modex;
    ucs_status_t ucs_status;
    uint8_t *modex_data;
    size_t msg_size;
    int rc;

    OPAL_THREAD_LOCK(&endpoint->ep_lock);
    if (NULL != endpoint->uct_eps[context_id]) {
        OPAL_THREAD_UNLOCK(&endpoint->ep_lock);
        return OPAL_SUCCESS;
    }

    do {
        OPAL_MODEX_RECV(rc, &mca_btl_uct_component.super.btl_version,
                        &endpoint->ep_proc->proc_name, (void **)&modex, &msg_size);
        if (OPAL_UNLIKELY(OPAL_SUCCESS != rc)) {
            BTL_ERROR(("error receiving modex"));
            break;
        }

        BTL_VERBOSE(("received modex of size %lu for proc %s", (unsigned long) msg_size,
                     OPAL_NAME_PRINT(endpoint->ep_proc->proc_name)));
        modex_data = modex->data;

        /* look for matching transport in the modex */
        for (int i = 0 ; i < modex->module_count ; ++i) {
            uint32_t modex_size = *((uint32_t *) modex_data);

            if (0 == strcmp (modex_data + 4, module->uct_tl_full_name)) {
                /* found it. locate the interface and device addresses. for now these are from
                 * the first context on the remote process. that is probably ok. */
                iface_addr = (uct_iface_addr_t *) (modex_data + 4 + strlen (module->uct_tl_full_name) + 1);
                device_addr = (uct_device_addr_t *) ((uintptr_t) iface_addr + module->uct_iface_attr.iface_addr_len);
                break;
            }

            modex_data += modex_size;
        }

        if (NULL == device_addr) {
            return OPAL_ERR_UNREACH;
        }

        /* try to connect the uct endpoint */
        ucs_status = uct_ep_create_connected (module->contexts[context_id].uct_iface, device_addr, iface_addr,
                                              endpoint->uct_eps + context_id);
        rc = UCS_OK == ucs_status ? OPAL_SUCCESS : OPAL_ERROR;
    } while (0);

    OPAL_THREAD_UNLOCK(&endpoint->ep_lock);

    if (OPAL_UNLIKELY(OPAL_SUCCESS != rc)) {
        BTL_VERBOSE(("error creating UCT endpoint from address. code: %d", ucs_status));
    }

    return rc;
}
