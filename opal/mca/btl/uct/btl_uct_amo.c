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

#include "btl_uct_rdma.h"

int mca_btl_uct_afop (struct mca_btl_base_module_t *btl, struct mca_btl_base_endpoint_t *endpoint,
                      void *local_address, uint64_t remote_address, mca_btl_base_registration_handle_t *local_handle,
                      mca_btl_base_registration_handle_t *remote_handle, mca_btl_base_atomic_op_t op,
                      uint64_t operand, int flags, int order, mca_btl_base_rdma_completion_fn_t cbfunc,
                      void *cbcontext, void *cbdata)
{
    mca_btl_uct_module_t *uct_module = (mca_btl_uct_module_t *) btl;
    mca_btl_uct_device_context_t *context = mca_btl_uct_module_get_context (uct_module);
    mca_btl_uct_uct_completion_t *comp = NULL;
    ucs_status_t ucs_status;
    uct_rkey_bundle_t rkey;
    uct_ep_h ep_handle;
    int rc;

    if (cbfunc) {
        comp = mca_btl_uct_uct_completion_alloc (btl, endpoint, local_address, local_handle, cbfunc, cbcontext, cbdata);
        if (OPAL_UNLIKELY(NULL == comp)) {
            return OPAL_ERR_OUT_OF_RESOURCE;
        }
    }

    rc = mca_btl_uct_get_rkey (uct_module, context, endpoint, remote_handle, &rkey, &ep_handle);
    if (OPAL_UNLIKELY(OPAL_SUCCESS != rc)) {
        mca_btl_uct_uct_completion_release (comp);
        return rc;
    }

    mca_btl_uct_context_lock (context);
    switch (op) {
    case MCA_BTL_ATOMIC_ADD:
        if (flags & MCA_BTL_ATOMIC_FLAG_32BIT) {
            ucs_status = uct_ep_atomic_fadd32 (ep_handle, (uint32_t) operand, remote_address,
                                               rkey.rkey, (uint32_t *) local_address, &comp->uct_comp);
        } else {
            ucs_status = uct_ep_atomic_fadd64 (ep_handle, operand, remote_address, rkey.rkey,
                                               (uint64_t *) local_address, &comp->uct_comp);
        }
        break;
    case MCA_BTL_ATOMIC_SWAP:
        if (flags & MCA_BTL_ATOMIC_FLAG_32BIT) {
            ucs_status = uct_ep_atomic_swap32 (ep_handle, (uint32_t) operand, remote_address,
                                               rkey.rkey, (uint32_t *) local_address, &comp->uct_comp);
        } else {
            ucs_status = uct_ep_atomic_swap64 (ep_handle, operand, remote_address, rkey.rkey,
                                               (uint64_t *) local_address, &comp->uct_comp);
        }
        break;
    default:
        mca_btl_uct_context_unlock (context);
        uct_rkey_release (&rkey);
        return OPAL_ERR_BAD_PARAM;
    }

    /* go ahead and progress the worker while we have the lock */
    (void) uct_worker_progress (context->uct_worker);

    mca_btl_uct_context_unlock (context);

    if (UCS_INPROGRESS == ucs_status) {
        rc = OPAL_SUCCESS;
    } else if (UCS_OK == ucs_status) {
        rc = 1;
    } else {
        rc = OPAL_ERR_OUT_OF_RESOURCE;
    }

    uct_rkey_release (&rkey);

    return rc;
}


int mca_btl_uct_aop (struct mca_btl_base_module_t *btl, mca_btl_base_endpoint_t *endpoint,
                     uint64_t remote_address, mca_btl_base_registration_handle_t *remote_handle,
                     mca_btl_base_atomic_op_t op, uint64_t operand, int flags, int order,
                     mca_btl_base_rdma_completion_fn_t cbfunc, void *cbcontext, void *cbdata)
{
    /* this is static so it survives after this function returns. we don't care about the result */
    static uint64_t result;

    /* just use the fetching ops for now. there probably is a performance benefit to using
     * the non-fetching on some platforms but this is easier to implement quickly and it
     * guarantees remote completion. */
    return mca_btl_uct_afop (btl, endpoint, &result, remote_address, NULL, remote_handle, op,
                             operand, flags, order, cbfunc, cbcontext, cbdata);
}

int mca_btl_uct_acswap (struct mca_btl_base_module_t *btl, struct mca_btl_base_endpoint_t *endpoint,
                        void *local_address, uint64_t remote_address, mca_btl_base_registration_handle_t *local_handle,
                        mca_btl_base_registration_handle_t *remote_handle, uint64_t compare, uint64_t value, int flags,
                        int order, mca_btl_base_rdma_completion_fn_t cbfunc, void *cbcontext, void *cbdata)
{
    mca_btl_uct_module_t *uct_module = (mca_btl_uct_module_t *) btl;
    mca_btl_uct_device_context_t *context = mca_btl_uct_module_get_context (uct_module);
    mca_btl_uct_uct_completion_t *comp = NULL;
    ucs_status_t ucs_status;
    uct_rkey_bundle_t rkey;
    uct_ep_h ep_handle;
    int rc;

    if (cbfunc) {
        comp = mca_btl_uct_uct_completion_alloc (btl, endpoint, local_address, local_handle, cbfunc, cbcontext, cbdata);
        if (OPAL_UNLIKELY(NULL == comp)) {
            return OPAL_ERR_OUT_OF_RESOURCE;
        }
    }

    rc = mca_btl_uct_get_rkey (uct_module, context, endpoint, remote_handle, &rkey, &ep_handle);
    if (OPAL_UNLIKELY(OPAL_SUCCESS != rc)) {
        mca_btl_uct_uct_completion_release (comp);
        return rc;
    }

    mca_btl_uct_context_lock (context);

    if (flags & MCA_BTL_ATOMIC_FLAG_32BIT) {
        ucs_status = uct_ep_atomic_cswap32 (ep_handle, (uint32_t) compare, (uint32_t) value, remote_address,
                                            rkey.rkey, (uint32_t *) local_address, &comp->uct_comp);
    } else {
        ucs_status = uct_ep_atomic_cswap64 (ep_handle, compare, value, remote_address, rkey.rkey,
                                            (uint64_t *) local_address, &comp->uct_comp);
    }

    /* go ahead and progress the worker while we have the lock */
    (void) uct_worker_progress (context->uct_worker);

    mca_btl_uct_context_unlock (context);

    if (UCS_INPROGRESS == ucs_status) {
        rc = OPAL_SUCCESS;
    } else if (UCS_OK == ucs_status) {
        rc = 1;
    } else {
        rc = OPAL_ERR_OUT_OF_RESOURCE;
    }

    uct_rkey_release (&rkey);

    return rc;
}
