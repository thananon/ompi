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

#include <rdma/fi_atomic.h>
#include "btl_ofi_rdma.h"

static int  to_fi_op(mca_btl_base_atomic_op_t op)
{
    switch (op) {
    case MCA_BTL_ATOMIC_ADD:
        return FI_SUM;
    case MCA_BTL_ATOMIC_SWAP:
        return FI_ATOMIC_WRITE;
    default:
        BTL_ERROR(("Unknown or unsupported atomic op."));
        abort();
    }
}

int mca_btl_ofi_afop (struct mca_btl_base_module_t *btl, struct mca_btl_base_endpoint_t *endpoint,
                      void *local_address, uint64_t remote_address, mca_btl_base_registration_handle_t *local_handle,
                      mca_btl_base_registration_handle_t *remote_handle, mca_btl_base_atomic_op_t op,
                      uint64_t operand, int flags, int order, mca_btl_base_rdma_completion_fn_t cbfunc,
                      void *cbcontext, void *cbdata)
{
    int rc;
    int fi_datatype = FI_UINT64;
    int fi_op;

    mca_btl_ofi_module_t *ofi_btl = (mca_btl_ofi_module_t *) btl;
    mca_btl_ofi_endpoint_t *btl_endpoint = (mca_btl_ofi_endpoint_t*) endpoint;
    mca_btl_ofi_completion_t *comp = NULL;

    if (flags & MCA_BTL_ATOMIC_FLAG_32BIT)
        fi_datatype = FI_UINT32;

    fi_op = to_fi_op(op);

    comp = mca_btl_ofi_completion_alloc(btl, endpoint,
                                        local_address,
                                        local_handle,
                                        cbfunc, cbcontext, cbdata,
                                        MCA_BTL_OFI_TYPE_AFOP);
    if (OPAL_UNLIKELY(NULL == comp)) {
        return OPAL_ERR_OUT_OF_RESOURCE;
    }

    rc = fi_fetch_atomic(ofi_btl->ofi_endpoint,
                         (void*) &operand, 1, NULL,             /* operand */
                         local_address, local_handle->desc,     /* results */
                         btl_endpoint->peer_addr,               /* remote addr */
                         remote_address, remote_handle->rkey,   /* remote buffer */
                         fi_datatype, fi_op, comp);

    if (rc == -FI_EAGAIN)
        return OPAL_ERR_OUT_OF_RESOURCE;
    else if (rc < 0) {
        BTL_ERROR(("fi_fetch_atomic failed with rc=%d (%s)", rc, fi_strerror(-rc)));
        abort();
    }

    OPAL_THREAD_ADD_FETCH64(&ofi_btl->outstanding_rdma, 1);
    return OPAL_SUCCESS;
}

int mca_btl_ofi_aop (struct mca_btl_base_module_t *btl, mca_btl_base_endpoint_t *endpoint,
                     uint64_t remote_address, mca_btl_base_registration_handle_t *remote_handle,
                     mca_btl_base_atomic_op_t op, uint64_t operand, int flags, int order,
                     mca_btl_base_rdma_completion_fn_t cbfunc, void *cbcontext, void *cbdata)
{
    int rc;
    int fi_datatype = FI_UINT64;
    int fi_op;

    mca_btl_ofi_module_t *ofi_btl = (mca_btl_ofi_module_t *) btl;
    mca_btl_ofi_endpoint_t *btl_endpoint = (mca_btl_ofi_endpoint_t*) endpoint;
    mca_btl_ofi_completion_t *comp = NULL;

    if (flags & MCA_BTL_ATOMIC_FLAG_32BIT)
        fi_datatype = FI_UINT32;

    fi_op = to_fi_op(op);

    comp = mca_btl_ofi_completion_alloc(btl, endpoint,
                                        NULL,
                                        NULL,
                                        cbfunc, cbcontext, cbdata,
                                        MCA_BTL_OFI_TYPE_AOP);
    if (OPAL_UNLIKELY(NULL == comp)) {
        return OPAL_ERR_OUT_OF_RESOURCE;
    }

    rc = fi_atomic(ofi_btl->ofi_endpoint,
                         (void*) &operand, 1, NULL,             /* operand */
                         btl_endpoint->peer_addr,               /* remote addr */
                         remote_address, remote_handle->rkey,   /* remote buffer */
                         fi_datatype, fi_op, comp);

    if (rc == -FI_EAGAIN)
        return OPAL_ERR_OUT_OF_RESOURCE;
    else if (rc < 0) {
        BTL_ERROR(("fi_fetch_atomic failed with rc=%d (%s)", rc, fi_strerror(-rc)));
        abort();
    }

    OPAL_THREAD_ADD_FETCH64(&ofi_btl->outstanding_rdma, 1);
    return OPAL_SUCCESS;
}

int mca_btl_ofi_acswap (struct mca_btl_base_module_t *btl, struct mca_btl_base_endpoint_t *endpoint,
                        void *local_address, uint64_t remote_address, mca_btl_base_registration_handle_t *local_handle,
                        mca_btl_base_registration_handle_t *remote_handle, uint64_t compare, uint64_t value, int flags,
                        int order, mca_btl_base_rdma_completion_fn_t cbfunc, void *cbcontext, void *cbdata)
{
    int rc;
    int fi_datatype = FI_UINT64;

    mca_btl_ofi_module_t *ofi_btl = (mca_btl_ofi_module_t *) btl;
    mca_btl_ofi_endpoint_t *btl_endpoint = (mca_btl_ofi_endpoint_t*) endpoint;
    mca_btl_ofi_completion_t *comp = NULL;

    if (flags & MCA_BTL_ATOMIC_FLAG_32BIT)
        fi_datatype = FI_UINT32;

    comp = mca_btl_ofi_completion_alloc(btl, endpoint,
                                        local_address,
                                        local_handle,
                                        cbfunc, cbcontext, cbdata,
                                        MCA_BTL_OFI_TYPE_CSWAP);
    if (OPAL_UNLIKELY(NULL == comp)) {
        return OPAL_ERR_OUT_OF_RESOURCE;
    }

    /* perform atomic */
    rc = fi_compare_atomic(ofi_btl->ofi_endpoint,
                           &value, 1, NULL,
                           &compare, NULL,
                           local_address, local_handle->desc,
                           btl_endpoint->peer_addr,
                           remote_address, remote_handle->rkey,
                           fi_datatype,
                           FI_CSWAP,
                           comp);

    if (rc == -FI_EAGAIN)
        return OPAL_ERR_OUT_OF_RESOURCE;
    else if (rc < 0) {
        BTL_ERROR(("fi_compare_atomic failed with rc=%d (%s)", rc, fi_strerror(-rc)));
        abort();
    }

    OPAL_THREAD_ADD_FETCH64(&ofi_btl->outstanding_rdma, 1);
    return OPAL_SUCCESS;

}
