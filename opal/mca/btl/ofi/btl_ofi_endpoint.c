/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2014-2018 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2018      Intel, Inc, All rights reserved
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "btl_ofi.h"
#include "btl_ofi_endpoint.h"
#include "opal/util/proc.h"

static void mca_btl_ofi_endpoint_construct (mca_btl_ofi_endpoint_t *endpoint)
{
    endpoint->peer_addr = 0;
    OBJ_CONSTRUCT(&endpoint->ep_lock, opal_mutex_t);
}

static void mca_btl_ofi_endpoint_destruct (mca_btl_ofi_endpoint_t *endpoint)
{
    endpoint->peer_addr = 0;

    /* set to null, we will free ofi endpoint in module */
    endpoint->ofi_endpoint = NULL;

    OBJ_DESTRUCT(&endpoint->ep_lock);
}

OBJ_CLASS_INSTANCE(mca_btl_ofi_endpoint_t, opal_list_item_t,
                   mca_btl_ofi_endpoint_construct,
                   mca_btl_ofi_endpoint_destruct);

mca_btl_base_endpoint_t *mca_btl_ofi_endpoint_create (opal_proc_t *proc, struct fid_ep *ep)
{
    mca_btl_ofi_endpoint_t *endpoint = OBJ_NEW(mca_btl_ofi_endpoint_t);

    if (OPAL_UNLIKELY(NULL == endpoint)) {
        return NULL;
    }

    endpoint->ep_proc = proc;
    endpoint->ofi_endpoint = ep;

    return (mca_btl_base_endpoint_t *) endpoint;
}

/* This function allocate communication contexts and return the pointer
 * to the first context. As of now, we need only transmit context. */
mca_btl_ofi_context_t *mca_btl_ofi_contexts_alloc(struct fi_info *info,
                                                  struct fid_domain *domain,
                                                  struct fid_ep *sep,
                                                  size_t num_contexts)
{
    assert(info);
    assert(domain);
    assert(sep);
    assert(num_contexts > 0);

    BTL_VERBOSE(("creating %zu contexts", num_contexts));

    int rc;
    size_t i;
    char *linux_device_name = info->domain_attr->name;

    struct fi_cq_attr cq_attr = {0};
    struct fi_tx_attr tx_attr = {0};
    struct fi_rx_attr rx_attr = {0};

    mca_btl_ofi_context_t *contexts;

    contexts = (mca_btl_ofi_context_t*) calloc(num_contexts, sizeof(*contexts));
    if (NULL == contexts) {
        BTL_VERBOSE(("cannot allocate communication contexts."));
        return NULL;
    }

    for (i=0; i < num_contexts; i++) {
        /* create transmit context */
        rc = fi_tx_context(sep, i, &tx_attr, &contexts[i].tx_ctx, NULL);
        if (0 != rc) {
            BTL_VERBOSE(("%s failed fi_tx_context with err=%s",
                            linux_device_name,
                            fi_strerror(-rc)
                            ));
            goto context_fail;
        }

        /* We don't actually need a receiving context as we only do one-sided.
         * However, sockets provider will hang if we dont have one. It is
         * also nice to have equal number of tx/rx context. */
        rc = fi_rx_context(sep, i, &rx_attr, &contexts[i].rx_ctx, NULL);
        if (0 != rc) {
            BTL_VERBOSE(("%s failed fi_rx_context with err=%s",
                            linux_device_name,
                            fi_strerror(-rc)
                            ));
            goto context_fail;
        }


        /* create CQ */
        cq_attr.format = FI_CQ_FORMAT_CONTEXT;
        cq_attr.wait_obj = FI_WAIT_NONE;
        rc = fi_cq_open(domain, &cq_attr, &contexts[i].cq, NULL);
        if (0 != rc) {
            BTL_VERBOSE(("%s failed fi_cq_open with err=%s",
                            linux_device_name,
                            fi_strerror(-rc)
                            ));
            goto context_fail;
        }

        /* bind cq to transmit context */
        uint32_t cq_flags = (FI_TRANSMIT);
        rc = fi_ep_bind(contexts[i].tx_ctx, (fid_t)contexts[i].cq, cq_flags);
        if (0 != rc) {
            BTL_VERBOSE(("%s failed fi_ep_bind with err=%s",
                            linux_device_name,
                            fi_strerror(-rc)
                            ));
            goto context_fail;
        }

        /* init free lists */
        OBJ_CONSTRUCT(&contexts[i].comp_list, opal_free_list_t);
        rc = opal_free_list_init(&contexts[i].comp_list,
                                 sizeof(mca_btl_ofi_completion_t),
                                 opal_cache_line_size,
                                 OBJ_CLASS(mca_btl_ofi_completion_t),
                                 0,
                                 0,
                                 128,
                                 -1,
                                 128,
                                 NULL,
                                 0,
                                 NULL,
                                 NULL,
                                 NULL);
        assert(OPAL_SUCCESS == rc);

        OBJ_CONSTRUCT(&contexts[i].lock, opal_mutex_t);

        /* assign the id */
        contexts[i].context_id = i;
    }
    return contexts;

context_fail:
    /* close and free */
    for(i=0; i < num_contexts; i++) {

        if (NULL != contexts[i].tx_ctx) {
            fi_close(&contexts[i].tx_ctx->fid);
        }

        if (NULL != contexts[i].rx_ctx) {
            fi_close(&contexts[i].rx_ctx->fid);
        }

        if(NULL != contexts[i].cq) {
            fi_close(&contexts[i].cq->fid);
        }
    }
    free(contexts);
    return NULL;
}

mca_btl_ofi_context_t *get_ofi_context(mca_btl_ofi_module_t *btl)
{
    static int cur_num = 0;
    return &btl->contexts[0];
    /** return &btl->contexts[cur_num++%btl->num_contexts]; */
}
