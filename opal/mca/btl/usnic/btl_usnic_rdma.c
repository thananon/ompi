/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006      Sandia National Laboratories. All rights
 *                         reserved.
 * Copyright (c) 2008-2015 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012      Los Alamos National Security, LLC.  All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "opal_config.h"

#include "opal_stdint.h"

#include "opal/constants.h"

#if BTL_IN_OPAL
#include "opal/mca/btl/btl.h"
#include "opal/mca/btl/base/base.h"
#else
#include "ompi/mca/btl/btl.h"
#include "ompi/mca/btl/base/base.h"
#endif

#include "btl_usnic.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_util.h"
#include "btl_usnic_rdma.h"

void
opal_btl_usnic_rdma_complete(opal_btl_usnic_module_t *module,
		            opal_btl_usnic_rdma_segment_t *seg){

    mca_btl_base_descriptor_t *desc = &seg->seg_desc;
    mca_btl_base_rdma_completion_fn_t func = (mca_btl_base_rdma_completion_fn_t) desc->des_cbfunc;

    /* Call the callback function */
    func(&module->super, seg->seg_endpoint, seg->local_address,seg->local_handle,
         desc->des_context, desc->des_cbdata, OPAL_SUCCESS);
    
    /* free the put segment */
    /* TODO : maybe make use of freelist structure */
    opal_btl_usnic_rdma_segment_return(module,seg);
}
