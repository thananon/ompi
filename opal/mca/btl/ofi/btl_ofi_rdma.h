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

#ifndef BTL_OFI_RDMA_H
#define BTL_OFI_RDMA_H

#include "btl_ofi.h"
#include "btl_ofi_endpoint.h"

mca_btl_ofi_completion_t *mca_btl_ofi_completion_alloc (
                                         mca_btl_base_module_t *btl,
                                         mca_btl_base_endpoint_t *endpoint,
                                         void *local_address,
                                         mca_btl_base_registration_handle_t *local_handle,
                                         mca_btl_base_rdma_completion_fn_t cbfunc,
                                         void *cbcontext, void *cbdata,
                                         int type);

#endif /* !defined(BTL_UCT_RDMA_H) */

