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

#if !defined(BTL_UCT_RDMA_H)
#define BTL_UCT_RDMA_H

#include "btl_uct.h"
#include "btl_uct_endpoint.h"

/**
 * @brief structure to keep track of btl callback
 *
 * This structuere is passed to various uct functions. It
 * does the translation between the uct callback and the
 * btl callback.
 */
struct mca_btl_uct_uct_completion_t {
    /** uct completion structure */
    uct_completion_t uct_comp;
    /** btl module associated with the callback */
    struct mca_btl_base_module_t *btl;
    /** btl endpoint associated with the callback */
    struct mca_btl_base_endpoint_t *endpoint;
    /** local address */
    void *local_address;
    /** local registration handle */
    mca_btl_base_registration_handle_t *local_handle;
    /** user callback function */
    mca_btl_base_rdma_completion_fn_t cbfunc;
    /** user callback context */
    void *cbcontext;
    /** user callback data */
    void *cbdata;
};
typedef struct mca_btl_uct_uct_completion_t mca_btl_uct_uct_completion_t;

/**
 * @brief allocate a callback structure
 */
mca_btl_uct_uct_completion_t *mca_btl_uct_uct_completion_alloc (mca_btl_base_module_t *btl, mca_btl_base_endpoint_t *endpoint,
                                                                void *local_address, mca_btl_base_registration_handle_t *local_handle,
                                                                mca_btl_base_rdma_completion_fn_t cbfunc,
                                                                void *cbcontext, void *cbdata);
/**
 * @brief release a callback structure
 */
void mca_btl_uct_uct_completion_release (mca_btl_uct_uct_completion_t *comp);

/**
 * @brief unpack the registration key and ensure the endpoint is connected
 *
 * @param[in]    module        uct btl module
 * @param[in]    context       device context to use
 * @param[in]    endpoint      btl endpoint
 * @param[in]    remote_handle buffer containing remote handle data
 * @param[inout] rkey          uct registration key bundle
 * @param[out]   ep_handle     uct endpoint handle
 */
static inline int mca_btl_uct_get_rkey (mca_btl_uct_module_t *module,
                                        mca_btl_uct_device_context_t *context,
                                        mca_btl_base_endpoint_t *endpoint,
					mca_btl_base_registration_handle_t *remote_handle,
                                        uct_rkey_bundle_t *rkey,
                                        uct_ep_h *ep_handle)
{
    ucs_status_t ucs_status;

    if (OPAL_SUCCESS != mca_btl_uct_endpoint_check_connect (module, endpoint, context)) {
        return OPAL_ERR_RESOURCE_BUSY;
    }

    *ep_handle = endpoint->uct_eps[context->context_id];

    ucs_status = uct_rkey_unpack ((void *) remote_handle, rkey);
    return (UCS_OK == ucs_status) ? OPAL_SUCCESS : OPAL_ERROR;
}

#endif /* !defined(BTL_UCT_RDMA_H) */
