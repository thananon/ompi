/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2017-2018 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCA_BTL_UCT_ENDPOINT_H
#define MCA_BTL_UCT_ENDPOINT_H

#include "opal/class/opal_list.h"
#include "opal/mca/event/event.h"
#include "btl_uct.h"

BEGIN_C_DECLS

struct mca_btl_base_endpoint_t {
    opal_list_item_t super;

    /** endpoint into UCT for this BTL endpoint */
    uct_ep_h uct_eps[MCA_BTL_UCT_MAX_WORKERS];

    /** endpoint proc */
    opal_proc_t *ep_proc;

    /** mutex to protect this structure */
    opal_mutex_t ep_lock;
};

typedef struct mca_btl_base_endpoint_t mca_btl_base_endpoint_t;
typedef mca_btl_base_endpoint_t mca_btl_uct_endpoint_t;
OBJ_CLASS_DECLARATION(mca_btl_uct_endpoint_t);

mca_btl_base_endpoint_t *mca_btl_uct_endpoint_create (opal_proc_t *proc);
int mca_btl_uct_endpoint_connect (mca_btl_uct_module_t *module, mca_btl_uct_endpoint_t *endpoint, int ep_index);

/**
 * @brief Check if the endpoint is connected and start the connection if not
 *
 * @param[in] endpoint  UCT BTL endpoint
 *
 * @returns OPAL_SUCCESS if the endpoint is connected and ready to us
 * @returns OPAL_ERR_RESOURCE_BUSY if the connection is underway
 * @returns OPAL_ERROR otherwise
 */
static inline int mca_btl_uct_endpoint_check_connect (mca_btl_uct_module_t *module, mca_btl_uct_endpoint_t *endpoint,
                                                      mca_btl_uct_device_context_t *context)
{
    int ep_index = context->context_id;

    if (NULL != endpoint->uct_eps[ep_index]) {
        return OPAL_SUCCESS;
    }

    return mca_btl_uct_endpoint_connect (module, endpoint, ep_index);
}

END_C_DECLS
#endif
