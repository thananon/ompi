/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2018 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2015-2018 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2018      Intel, Inc, All rights reserved
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/**
 * @file
 */
#ifndef MCA_BTL_OFI_H
#define MCA_BTL_OFI_H

#include "opal_config.h"
#include <sys/types.h>
#include <string.h>

/* Open MPI includes */
#include "opal/mca/event/event.h"
#include "opal/mca/btl/btl.h"
#include "opal/mca/btl/base/base.h"
#include "opal/mca/mpool/mpool.h"
#include "opal/mca/btl/base/btl_base_error.h"
#include "opal/mca/rcache/base/base.h"
#include "opal/mca/pmix/pmix.h"

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_rma.h>

BEGIN_C_DECLS

#define MCA_BTL_OFI_MAX_MODULES 16
#define MCA_BTL_OFI_MAX_WORKERS 1
#define MCA_BTL_OFI_MAX_CQ_READ_ENTRIES 128

#define MCA_BTL_OFI_ABORT(args)     mca_btl_ofi_exit(args)

enum mca_btl_ofi_type {
    MCA_BTL_OFI_TYPE_PUT = 1,
    MCA_BTL_OFI_TYPE_GET,
    MCA_BTL_OFI_TYPE_AOP,
    MCA_BTL_OFI_TYPE_AFOP,
    MCA_BTL_OFI_TYPE_CSWAP,
    MCA_BTL_OFI_TYPE_TOTAL
};

struct mca_btl_ofi_context_t {
    int32_t context_id;

    /* transmit context */
    struct fid_ep *tx_ctx;
    struct fid_ep *rx_ctx;

    /* completion queue */
    struct fid_cq *cq;

    /* completion info freelist */
    /* We have it per context to reduce the thread contention
     * on the freelist. Things can get really slow. */
    opal_free_list_t comp_list;

    opal_mutex_t lock;
};
typedef struct mca_btl_ofi_context_t mca_btl_ofi_context_t;

/**
 * @brief OFI BTL module
 */
struct mca_btl_ofi_module_t {
    /** base BTL interface */
    mca_btl_base_module_t super;

    /* libfabric components */
    struct fi_info *fabric_info;
    struct fid_fabric *fabric;
    struct fid_domain *domain;
    struct fid_ep *ofi_endpoint;
    struct fid_av *av;

    int num_contexts;
    mca_btl_ofi_context_t *contexts;

    char *linux_device_name;

    /** whether the module has been fully initialized or not */
    bool initialized;
    bool use_virt_addr;
    bool is_scalable_ep;

    /** spin-lock to protect the module */
    volatile int32_t lock;

    int64_t outstanding_rdma;

    /** linked list of BTL endpoints. this list is never searched so
     * there is no need for a complicated structure here at this time*/
    opal_list_t endpoints;

    /** registration cache */
    mca_rcache_base_module_t *rcache;
};
typedef struct mca_btl_ofi_module_t mca_btl_ofi_module_t;

extern mca_btl_ofi_module_t mca_btl_ofi_module_template;

/**
 * @brief OFI BTL component
 */
struct mca_btl_ofi_component_t {
    mca_btl_base_component_3_0_0_t super;  /**< base BTL component */

    /** number of TL modules */
    int module_count;
    int num_contexts_per_module;
    int num_cqe_read;

    size_t namelen;

    /** All BTL OFI modules (1 per tl) */
    mca_btl_ofi_module_t *modules[MCA_BTL_OFI_MAX_MODULES];

#if OPAL_C_HAVE__THREAD_LOCAL
    /** bind threads to contexts */
    bool bind_threads_to_contexts;
#endif
};
typedef struct mca_btl_ofi_component_t mca_btl_ofi_component_t;

OPAL_MODULE_DECLSPEC extern mca_btl_ofi_component_t mca_btl_ofi_component;

struct mca_btl_base_registration_handle_t {
    uint64_t rkey;
    void *desc;
    void *base_addr;
};

struct mca_btl_ofi_reg_t {
    mca_rcache_base_registration_t base;
    struct fid_mr *ur_mr;

    /* remote handle */
    mca_btl_base_registration_handle_t handle;
};
typedef struct mca_btl_ofi_reg_t mca_btl_ofi_reg_t;

OBJ_CLASS_DECLARATION(mca_btl_ofi_reg_t);

/* completion structure store information needed
 * for RDMA callbacks */
struct mca_btl_ofi_completion_t {
    opal_free_list_item_t comp_list;
    opal_free_list_t *my_list;

    struct mca_btl_base_module_t *btl;
    struct mca_btl_base_endpoint_t *endpoint;
    struct mca_btl_ofi_context_t *my_context;
    uint32_t type;

    void *local_address;
    mca_btl_base_registration_handle_t *local_handle;

    /* information for atomic op */
    uint64_t operand;
    uint64_t compare;

    mca_btl_base_rdma_completion_fn_t cbfunc;
    void *cbcontext;
    void *cbdata;

};
typedef struct mca_btl_ofi_completion_t mca_btl_ofi_completion_t;

OBJ_CLASS_DECLARATION(mca_btl_ofi_completion_t);

/**
 * Initiate an asynchronous put.
 * Completion Semantics: if this function returns a 1 then the operation
 *                       is complete. a return of OPAL_SUCCESS indicates
 *                       the put operation has been queued with the
 *                       network. the local_handle can not be deregistered
 *                       until all outstanding operations on that handle
 *                       have been completed.
 *
 * @param btl (IN)            BTL module
 * @param endpoint (IN)       BTL addressing information
 * @param local_address (IN)  Local address to put from (registered)
 * @param remote_address (IN) Remote address to put to (registered remotely)
 * @param local_handle (IN)   Registration handle for region containing
 *                            (local_address, local_address + size)
 * @param remote_handle (IN)  Remote registration handle for region containing
 *                            (remote_address, remote_address + size)
 * @param size (IN)           Number of bytes to put
 * @param flags (IN)          Flags for this put operation
 * @param order (IN)          Ordering
 * @param cbfunc (IN)         Function to call on completion (if queued)
 * @param cbcontext (IN)      Context for the callback
 * @param cbdata (IN)         Data for callback
 *
 * @retval OPAL_SUCCESS    The descriptor was successfully queued for a put
 * @retval OPAL_ERROR      The descriptor was NOT successfully queued for a put
 * @retval OPAL_ERR_OUT_OF_RESOURCE  Insufficient resources to queue the put
 *                         operation. Try again later
 * @retval OPAL_ERR_NOT_AVAILABLE  Put can not be performed due to size or
 *                         alignment restrictions.
 */
int mca_btl_ofi_put (struct mca_btl_base_module_t *btl,
    struct mca_btl_base_endpoint_t *endpoint, void *local_address,
    uint64_t remote_address, struct mca_btl_base_registration_handle_t *local_handle,
    struct mca_btl_base_registration_handle_t *remote_handle, size_t size, int flags,
    int order, mca_btl_base_rdma_completion_fn_t cbfunc, void *cbcontext, void *cbdata);

/**
 * Initiate an asynchronous get.
 * Completion Semantics: if this function returns a 1 then the operation
 *                       is complete. a return of OPAL_SUCCESS indicates
 *                       the get operation has been queued with the
 *                       network. the local_handle can not be deregistered
 *                       until all outstanding operations on that handle
 *                       have been completed.
 *
 * @param btl (IN)            BTL module
 * @param endpoint (IN)       BTL addressing information
 * @param local_address (IN)  Local address to put from (registered)
 * @param remote_address (IN) Remote address to put to (registered remotely)
 * @param local_handle (IN)   Registration handle for region containing
 *                            (local_address, local_address + size)
 * @param remote_handle (IN)  Remote registration handle for region containing
 *                            (remote_address, remote_address + size)
 * @param size (IN)           Number of bytes to put
 * @param flags (IN)          Flags for this put operation
 * @param order (IN)          Ordering
 * @param cbfunc (IN)         Function to call on completion (if queued)
 * @param cbcontext (IN)      Context for the callback
 * @param cbdata (IN)         Data for callback
 *
 * @retval OPAL_SUCCESS    The descriptor was successfully queued for a put
 * @retval OPAL_ERROR      The descriptor was NOT successfully queued for a put
 * @retval OPAL_ERR_OUT_OF_RESOURCE  Insufficient resources to queue the put
 *                         operation. Try again later
 * @retval OPAL_ERR_NOT_AVAILABLE  Put can not be performed due to size or
 *                         alignment restrictions.
 */
int mca_btl_ofi_get (struct mca_btl_base_module_t *btl,
    struct mca_btl_base_endpoint_t *endpoint, void *local_address,
    uint64_t remote_address, struct mca_btl_base_registration_handle_t *local_handle,
    struct mca_btl_base_registration_handle_t *remote_handle, size_t size, int flags,
    int order, mca_btl_base_rdma_completion_fn_t cbfunc, void *cbcontext, void *cbdata);

int mca_btl_ofi_aop (struct mca_btl_base_module_t *btl, struct mca_btl_base_endpoint_t *endpoint,
                     uint64_t remote_address, mca_btl_base_registration_handle_t *remote_handle,
                     mca_btl_base_atomic_op_t op, uint64_t operand, int flags, int order,
                     mca_btl_base_rdma_completion_fn_t cbfunc, void *cbcontext, void *cbdata);

int mca_btl_ofi_afop (struct mca_btl_base_module_t *btl, struct mca_btl_base_endpoint_t *endpoint,
                      void *local_address, uint64_t remote_address, mca_btl_base_registration_handle_t *local_handle,
                      mca_btl_base_registration_handle_t *remote_handle, mca_btl_base_atomic_op_t op,
                      uint64_t operand, int flags, int order, mca_btl_base_rdma_completion_fn_t cbfunc,
                      void *cbcontext, void *cbdata);

int mca_btl_ofi_acswap (struct mca_btl_base_module_t *btl, struct mca_btl_base_endpoint_t *endpoint,
                        void *local_address, uint64_t remote_address, mca_btl_base_registration_handle_t *local_handle,
                        mca_btl_base_registration_handle_t *remote_handle, uint64_t compare, uint64_t value, int flags,
                        int order, mca_btl_base_rdma_completion_fn_t cbfunc, void *cbcontext, void *cbdata);


int mca_btl_ofi_flush (struct mca_btl_base_module_t *btl, struct mca_btl_base_endpoint_t *endpoint);

int mca_btl_ofi_finalize (mca_btl_base_module_t *btl);

void mca_btl_ofi_rcache_init (mca_btl_ofi_module_t *module);
int mca_btl_ofi_reg_mem (void *reg_data, void *base, size_t size,
                         mca_rcache_base_registration_t *reg);
int mca_btl_ofi_dereg_mem (void *reg_data, mca_rcache_base_registration_t *reg);

void mca_btl_ofi_exit(void);

END_C_DECLS
#endif
