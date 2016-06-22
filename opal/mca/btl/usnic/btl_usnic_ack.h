/*
 * Copyright (c) 2013-2017 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef BTL_USNIC_ACK_H
#define BTL_USNIC_ACK_H

#include "opal_config.h"

#include "opal/class/opal_hotel.h"

#include "btl_usnic.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_endpoint.h"
#include "btl_usnic_compat.h"

/* Invoke the descriptor callback for a (non-PUT) send frag, updating
 * stats and clearing the _CALLBACK flag in the process. */
#define OPAL_BTL_USNIC_DO_SEND_FRAG_CB(module, send_frag, comment)            \
    do {                                                                      \
        MSGDEBUG1_OUT("%s:%d: %s SEND callback for module=%p frag=%p\n",      \
                      __func__, __LINE__,                                     \
                      (comment), (void *)(module), (void *)(send_frag));      \
        (send_frag)->sf_base.uf_base.des_cbfunc(                              \
            &(module)->super,                                                 \
            (send_frag)->sf_endpoint,                                         \
            &(send_frag)->sf_base.uf_base,                                    \
            OPAL_SUCCESS);                                                    \
        frag->sf_base.uf_base.des_flags &= ~MCA_BTL_DES_SEND_ALWAYS_CALLBACK; \
        ++((module)->stats.pml_send_callbacks);                               \
    } while (0)

#if BTL_VERSION == 30
/* Invoke the descriptor callback for a send frag that was a PUT,
 * updating stats and clearing the _CALLBACK flag in the process. */
#define OPAL_BTL_USNIC_DO_PUT_FRAG_CB(module, send_frag, comment)             \
    do {                                                                      \
        MSGDEBUG1_OUT("%s:%d: %s PUT callback for module=%p frag=%p\n",       \
                      __func__, __LINE__,                                     \
                      (comment), (void *)(module), (void *)(send_frag));      \
        mca_btl_base_rdma_completion_fn_t func =                        \
            (mca_btl_base_rdma_completion_fn_t)                         \
            (send_frag)->sf_base.uf_base.des_cbfunc;                    \
        func(&(module)->super,                                          \
             (send_frag)->sf_endpoint,                                  \
             (send_frag)->sf_base.uf_local_seg[0].seg_addr.pval,        \
             NULL,                                                      \
             (send_frag)->sf_base.uf_base.des_context,                  \
             (send_frag)->sf_base.uf_base.des_cbdata,                   \
             OPAL_SUCCESS);                                             \
        ++((module)->stats.pml_send_callbacks);                         \
    } while (0)
#endif

#endif /* BTL_USNIC_ACK_H */
