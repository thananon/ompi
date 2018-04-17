/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2009 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2015-2018 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/**
 * @file
 */
#ifndef MCA_BTL_UCT_H
#define MCA_BTL_UCT_H

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
#include <ucp/api/ucp.h>
#include <uct/api/uct.h>

BEGIN_C_DECLS

#define MCA_BTL_UCT_TL_NAME_MAX 64
#define MCA_BTL_UCT_ADDRESS_MAX 64
#define MCA_BTL_UCT_MAX_MODULES 16
#define MCA_BTL_UCT_MAX_WORKERS 64

enum mca_btl_uct_am_id {
    UCT_AM_ID_RECV_COMPLETE
};

struct mca_btl_uct_modex_t {
    int32_t module_count;
    uint8_t data[];
};
typedef struct mca_btl_uct_modex_t mca_btl_uct_modex_t;

struct mca_btl_uct_md_t {
    opal_object_t super;
    uct_md_h uct_md;
};
typedef struct mca_btl_uct_md_t mca_btl_uct_md_t;

OBJ_CLASS_DECLARATION(mca_btl_uct_md_t);

struct mca_btl_uct_device_context_t {
    volatile int32_t lock;
    int context_id;
    uct_worker_h uct_worker;
    uct_iface_h uct_iface;
};
typedef struct mca_btl_uct_device_context_t mca_btl_uct_device_context_t;

/**
 * @brief UCT BTL module
 */
struct mca_btl_uct_module_t {
    /** base BTL interface */
    mca_btl_base_module_t super;

    /** whether the module has been fully initialized or not */
    bool initialized;

    /** spin-lock to protect the module */
    volatile int32_t lock;

    /** memory domain associated with this module (there may be multiple modules using the same domain) */
    mca_btl_uct_md_t *uct_md;

    /** async context */
    ucs_async_context_t *ucs_async;

    /** number of worker contexts */
    int uct_worker_count;

    /** device contexts into UCT */
    mca_btl_uct_device_context_t contexts[MCA_BTL_UCT_MAX_WORKERS];

    /** linked list of BTL endpoints. this list is never searched so
     * there is no need for a complicated structure here at this time*/
    opal_list_t endpoints;

    /** transport layer name */
    char *uct_tl_full_name;

    /** transport layer name */
    char *uct_tl_name;

    /** interface device name */
    char *uct_dev_name;

    /** registration cache */
    mca_rcache_base_module_t *rcache;

    /** interface attributes */
    uct_iface_attr_t uct_iface_attr;

    /** UCT transport layer configuration */
    uct_iface_config_t *uct_tl_config;

    /** uct endpoint: experimental **/
    uct_ep_h uct_endpoint;

    opal_free_list_t recv_frag_list;
};
typedef struct mca_btl_uct_module_t mca_btl_uct_module_t;

extern mca_btl_uct_module_t mca_btl_uct_module_template;

/**
 * @brief UCT BTL component
 */
struct mca_btl_uct_component_t {
    mca_btl_base_component_3_0_0_t super;  /**< base BTL component */

    /** number of TL modules */
    int module_count;

    /** All BTL UCT modules (1 per tl) */
    mca_btl_uct_module_t *modules[MCA_BTL_UCT_MAX_MODULES];

    /** allowed UCT transport methods */
    char *transports;

    /** number of worker contexts to create */
    int num_contexts_per_module;

#if OPAL_C_HAVE__THREAD_LOCAL
    /** bind threads to contexts */
    bool bind_threads_to_contexts;
#endif
};
typedef struct mca_btl_uct_component_t mca_btl_uct_component_t;

OPAL_MODULE_DECLSPEC extern mca_btl_uct_component_t mca_btl_uct_component;

struct mca_btl_base_registration_handle_t {
    /** The packed memory handle. The size of this field is defined by UCT. */
    uint8_t packed_handle[1];
};

struct mca_btl_uct_reg_t {
    mca_rcache_base_registration_t base;

    /** UCT memory handle */
    uct_mem_h uct_memh;

    /** remote handle */
    mca_btl_base_registration_handle_t handle;
};
typedef struct mca_btl_uct_reg_t mca_btl_uct_reg_t;

OBJ_CLASS_DECLARATION(mca_btl_uct_reg_t);

int mca_btl_uct_send( struct mca_btl_base_module_t* btl,
                      struct mca_btl_base_endpoint_t* endpoint,
                      struct mca_btl_base_descriptor_t* descriptor,
                      mca_btl_base_tag_t tag );

#define MCA_BTL_UCT_REG_REMOTE_TO_LOCAL(reg) ((mca_btl_uct_reg_t *)((intptr_t) (reg) - offsetof (mca_btl_uct_reg_t, handle)))

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
int mca_btl_uct_put (struct mca_btl_base_module_t *btl,
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
int mca_btl_uct_get (struct mca_btl_base_module_t *btl,
    struct mca_btl_base_endpoint_t *endpoint, void *local_address,
    uint64_t remote_address, struct mca_btl_base_registration_handle_t *local_handle,
    struct mca_btl_base_registration_handle_t *remote_handle, size_t size, int flags,
    int order, mca_btl_base_rdma_completion_fn_t cbfunc, void *cbcontext, void *cbdata);

 /**
  * Fault Tolerance Event Notification Function
  * @param state Checkpoint Stae
  * @return OPAL_SUCCESS or failure status
  */
int mca_btl_uct_ft_event(int state);

int mca_btl_uct_aop (struct mca_btl_base_module_t *btl, struct mca_btl_base_endpoint_t *endpoint,
                     uint64_t remote_address, mca_btl_base_registration_handle_t *remote_handle,
                     mca_btl_base_atomic_op_t op, uint64_t operand, int flags, int order,
                     mca_btl_base_rdma_completion_fn_t cbfunc, void *cbcontext, void *cbdata);

int mca_btl_uct_afop (struct mca_btl_base_module_t *btl, struct mca_btl_base_endpoint_t *endpoint,
                      void *local_address, uint64_t remote_address, mca_btl_base_registration_handle_t *local_handle,
                      mca_btl_base_registration_handle_t *remote_handle, mca_btl_base_atomic_op_t op,
                      uint64_t operand, int flags, int order, mca_btl_base_rdma_completion_fn_t cbfunc,
                      void *cbcontext, void *cbdata);

int mca_btl_uct_acswap (struct mca_btl_base_module_t *btl, struct mca_btl_base_endpoint_t *endpoint,
                        void *local_address, uint64_t remote_address, mca_btl_base_registration_handle_t *local_handle,
                        mca_btl_base_registration_handle_t *remote_handle, uint64_t compare, uint64_t value, int flags,
                        int order, mca_btl_base_rdma_completion_fn_t cbfunc, void *cbcontext, void *cbdata);


int mca_btl_uct_flush (struct mca_btl_base_module_t *btl, struct mca_btl_base_endpoint_t *endpoint);

/**
 * @brief initialize a uct btl device context
 *
 * @param[in] module    uct btl module
 * @param[in] context   uct device context to initialize
 *
 * @returns OPAL_SUCCESS on success
 * @returns opal error code on failure
 */
int mca_btl_uct_context_init (mca_btl_uct_module_t *module, mca_btl_uct_device_context_t *context);
void mca_btl_uct_context_fini (mca_btl_uct_module_t *module, mca_btl_uct_device_context_t *context);
int mca_btl_uct_finalize (mca_btl_base_module_t *btl);

static inline bool mca_btl_uct_context_trylock (mca_btl_uct_device_context_t *context)
{
    return (context->lock || OPAL_ATOMIC_SWAP_32 (&context->lock, 1));
}

static inline void mca_btl_uct_context_lock (mca_btl_uct_device_context_t *context)
{
    while (mca_btl_uct_context_trylock (context));
}

static inline void mca_btl_uct_context_unlock (mca_btl_uct_device_context_t *context)
{
    opal_atomic_mb ();
    context->lock = 0;
}

static inline int mca_btl_uct_context_progress (mca_btl_uct_device_context_t *context)
{
    int ret;

    if (context->uct_worker) {
        mca_btl_uct_context_lock (context);
        ret = uct_worker_progress (context->uct_worker);
        mca_btl_uct_context_unlock (context);
    }

    return ret;
}

static inline int mca_btl_uct_get_context_index (mca_btl_uct_module_t *module)
{
    static volatile uint32_t next_uct_index = 0;
    int context_id;

#if OPAL_C_HAVE__THREAD_LOCAL
    if (mca_btl_uct_component.bind_threads_to_contexts) {
        static _Thread_local int uct_index = -1;

        context_id = uct_index;
        if (OPAL_UNLIKELY(-1 == context_id)) {
            context_id = uct_index = opal_atomic_fetch_add_32 ((volatile int32_t *) &next_uct_index, 1) % module->uct_worker_count;
        }
    } else {
#endif
        /* avoid using atomics in this. i doubt it improves performance to ensure atomicity on the next
         * index in this case. */
        context_id = next_uct_index++ % module->uct_worker_count;
#if OPAL_C_HAVE__THREAD_LOCAL
    }
#endif

    return context_id;
}

static inline mca_btl_uct_device_context_t *mca_btl_uct_module_get_context (mca_btl_uct_module_t *module)
{
    mca_btl_uct_device_context_t *context = module->contexts + mca_btl_uct_get_context_index (module);

    if (NULL == context->uct_worker) {
        /* set up this context */
        mca_btl_uct_context_init (module, context);
    }

    return context;
}

END_C_DECLS
#endif
