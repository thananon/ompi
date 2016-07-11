/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef BTL_USNIC_RDMA_H
#define BTL_USNIC_RDMA_H

#include "btl_usnic.h"
#include "btl_usnic_frag.h"


void opal_btl_usnic_rdma_complete(opal_btl_usnic_module_t *module,
		            opal_btl_usnic_put_segment_t *pseg);

#endif
