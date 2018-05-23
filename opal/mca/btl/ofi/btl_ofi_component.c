/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
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


#include "opal_config.h"

#include "opal/mca/btl/btl.h"
#include "opal/mca/btl/base/base.h"
#include "opal/mca/hwloc/base/base.h"

#include <string.h>

#include "btl_ofi.h"
#include "btl_ofi_providers.h"


static char *prov_include;
static char *prov_exclude;
static int mca_btl_ofi_init_device(struct fi_info *info);

static int mca_btl_ofi_component_register(void)
{
    mca_btl_ofi_module_t *module = &mca_btl_ofi_module_template;

    /* fi_getinfo with prov_name == NULL means ALL provider */
    prov_include = NULL;
    (void) mca_base_component_var_register(&mca_btl_ofi_component.super.btl_version,
                                          "provider_include",
                                          "Comma-delimited list of OFI providers that are considered for use "
                                          "(e.g., \"psm,psm2\"; an empty value means that all providers will "
                                          "be considered)."
                                          " Mutually exclusive with btl_ofi_provider_exclude.",
                                          MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                          OPAL_INFO_LVL_1,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &prov_include);

    prov_exclude = NULL;
    (void) mca_base_component_var_register(&mca_btl_ofi_component.super.btl_version,
                                          "provider_exclude",
                                          "Comma-delimited list of OFI providers that are not considered for use "
                                          "(default: \"sockets,mxm\"; empty value means that all providers will "
                                          " be considered). "
                                          "Mutually exclusive with btl_ofi_provider_include.",
                                          MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                          OPAL_INFO_LVL_1,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &prov_exclude);
    /* Note: better leave it at 1 for now. osc rdma module is designed for 1 completion
     * at a time. Dealing with more than 1 completion in 1 read will confuse the osc rdma.
     * source: 8 hours of debugging. :(*/
    mca_btl_ofi_component.num_cqe_read = 1;
    (void) mca_base_component_var_register(&mca_btl_ofi_component.super.btl_version,
                                          "num_cq_read",
                                          "Number of completion entries to read from a single cq_read. "
                                          "(default: 1)",
                                          MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                          OPAL_INFO_LVL_2,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &mca_btl_ofi_component.num_cqe_read);


#if OPAL_C_HAVE__THREAD_LOCAL
    mca_btl_ofi_component.bind_threads_to_contexts = true;
    (void) mca_base_component_var_register(&mca_btl_ofi_component.super.btl_version,
                                           "bind_threads_to_contexts", "Bind threads to device contexts. "
                                           "In general this should improve the multi-threaded performance "
                                           "when threads are used. (default: true)", MCA_BASE_VAR_TYPE_BOOL,
                                           NULL, 0 ,MCA_BASE_VAR_FLAG_SETTABLE, OPAL_INFO_LVL_3,
                                           MCA_BASE_VAR_SCOPE_ALL,
                                           &mca_btl_ofi_component.bind_threads_to_contexts);
#endif

    /* for now we want this component to lose to btl/ugni and btl/vader */
    module->super.btl_exclusivity = MCA_BTL_EXCLUSIVITY_HIGH - 50;

    return mca_btl_base_param_register (&mca_btl_ofi_component.super.btl_version,
                                        &module->super);
}

static int mca_btl_ofi_component_open(void)
{
    mca_btl_ofi_component.module_count = 0;
    return OPAL_SUCCESS;
}


/*
 * component cleanup - sanity checking of queue lengths
 */
static int mca_btl_ofi_component_close(void)
{
    /* if we dont sleep, the threads freak out */
    sleep(1);
    return OPAL_SUCCESS;
}

/*
 *  OFI component initialization:
 *  (1) read interface list from kernel and compare against component parameters
 *      then create a BTL instance for selected interfaces
 *  (2) setup OFI listen socket for incoming connection attempts
 *  (3) register BTL parameters with the MCA
 */

static mca_btl_base_module_t **mca_btl_ofi_component_init (int *num_btl_modules, bool enable_progress_threads,
                                                           bool enable_mpi_threads)
{
    /* for this BTL to be useful the interface needs to support RDMA and certain atomic operations */
    struct mca_btl_base_module_t **base_modules;
    unsigned resource_count = 0;
    int rc;
    int requested_mr_mode = MCA_BTL_OFI_REQUESTED_MR_MODE;

    BTL_VERBOSE(("initializing ofi btl"));

    /* Set up libfabric hints. */
    uint32_t libfabric_api;
    libfabric_api = FI_VERSION(1, 5);       /* 1.5 because of the newer API */

    struct fi_info *info, *info_list;
    struct fi_info hints = {0};
    struct fi_ep_attr ep_attr = {0};
    struct fi_rx_attr rx_attr = {0};
    struct fi_tx_attr tx_attr = {0};
    struct fi_fabric_attr fabric_attr = {0};
    struct fi_domain_attr domain_attr = {0};

    /* Select the provider */
    fabric_attr.prov_name = prov_include;

    domain_attr.mr_mode = requested_mr_mode;

    /* will deal with this later */
    /** domain_attr.control_progress = FI_PROGRESS_AUTO; */
    /** domain_attr.data_progress = FI_PROGRESS_AUTO; */

    /* select endpoint type */
    ep_attr.type = FI_EP_RDM;

    /* ask for capabilities */
    hints.caps = MCA_BTL_OFI_REQUIRED_CAPS;

    hints.fabric_attr = &fabric_attr;
    hints.domain_attr = &domain_attr;
    hints.ep_attr = &ep_attr;
    hints.tx_attr = &tx_attr;
    hints.rx_attr = &rx_attr;

    /* for now */
    tx_attr.iov_limit = 1;
    rx_attr.iov_limit = 1;

    /* handle provider specific hints here */
    provider_hints_handler(&hints);

    mca_btl_ofi_component.module_count = 0;

    /* do the query. */
    rc = fi_getinfo(libfabric_api, NULL, NULL, 0, &hints, &info_list);
    if (rc != 0) {
        BTL_VERBOSE(("fi_getinfo failed with code %d: %s",rc, fi_strerror(-rc)));
        return NULL;
    }

    /* count the number of resources/ */
    info = info_list;
    while(info) {
        resource_count++;
        info = info->next;
    }
    BTL_VERBOSE(("ofi btl found %d possible resources.", resource_count));

    info = info_list;

    while(info->next || info) {
        rc = validate_info(info);
        if (OPAL_SUCCESS == rc) {
            /* Device passed sanity check, let's make a module.
             * We only pick the first device we found valid */
            rc = mca_btl_ofi_init_device(info);
            if (OPAL_SUCCESS == rc)
                break;
        }
        info = info->next;
    }
    /* pass module array back to caller */
    base_modules = calloc (mca_btl_ofi_component.module_count, sizeof (*base_modules));
    if (NULL == base_modules) {
        return NULL;
    }

    memcpy (base_modules, mca_btl_ofi_component.modules,
            mca_btl_ofi_component.module_count *sizeof (mca_btl_ofi_component.modules[0]));

    BTL_VERBOSE(("ofi btl initialization complete. found %d suitable transports",
                 mca_btl_ofi_component.module_count));

    *num_btl_modules = mca_btl_ofi_component.module_count;

    fi_freeinfo(info_list);

    return base_modules;
}

static int mca_btl_ofi_init_device(struct fi_info *info)
{
    int rc;
    int *module_count = &mca_btl_ofi_component.module_count;
    size_t namelen;
    mca_btl_ofi_module_t *module;

    char *linux_device_name;
    char ep_name[FI_NAME_MAX];
    struct fi_info *ofi_info;
    struct fi_cq_attr cq_attr = {0};
    struct fi_av_attr av_attr = {0};
    struct fid_fabric *fabric;
    struct fid_domain *domain;
    struct fid_ep *endpoint;
    struct fid_cq *cq;
    struct fid_av *av;

    /* make a copy of the given info to store on the module */
    ofi_info = fi_dupinfo(info);

    linux_device_name = info->domain_attr->name;
    BTL_VERBOSE(("initializing dev:%s provider:%s",
                    linux_device_name,
                    info->fabric_attr->prov_name));

    /* fabric */
    rc = fi_fabric(ofi_info->fabric_attr, &fabric, NULL);
    if (0 != rc) {
        BTL_VERBOSE(("%s failed fi_fabric with err=%s",
                        linux_device_name,
                        fi_strerror(-rc)
                        ));
        goto fail;
    }

    /* domain */
    rc = fi_domain(fabric, ofi_info, &domain, NULL);
    if (0 != rc) {
        BTL_VERBOSE(("%s failed fi_domain with err=%s",
                        linux_device_name,
                        fi_strerror(-rc)
                        ));
        goto fail;
    }

    /* endpoint */
    rc = fi_endpoint(domain, ofi_info, &endpoint, NULL);
    if (0 != rc) {
        BTL_VERBOSE(("%s failed fi_endpoint with err=%s",
                        linux_device_name,
                        fi_strerror(-rc)
                        ));
        goto fail;
    }

    /* CQ */
    cq_attr.format = FI_CQ_FORMAT_CONTEXT;
    cq_attr.wait_obj = FI_WAIT_NONE;
    rc = fi_cq_open(domain, &cq_attr, &cq, NULL);
    if (0 != rc) {
        BTL_VERBOSE(("%s failed fi_cq_open with err=%s",
                        linux_device_name,
                        fi_strerror(-rc)
                        ));
        goto fail;
    }

    /* AV */
    av_attr.type = FI_AV_MAP;
    rc = fi_av_open(domain, &av_attr, &av, NULL);
    if (0 != rc) {
        BTL_VERBOSE(("%s failed fi_av_open with err=%s",
                        linux_device_name,
                        fi_strerror(-rc)
                        ));
        goto fail;
    }


    /* bind CQ and AV to endpoint */
    uint32_t cq_flags = (FI_TRANSMIT);
    rc = fi_ep_bind(endpoint, (fid_t)cq, cq_flags);
    if (0 != rc) {
        BTL_VERBOSE(("%s failed fi_ep_bind with err=%s",
                        linux_device_name,
                        fi_strerror(-rc)
                        ));
        goto fail;
    }

    rc = fi_ep_bind(endpoint, (fid_t)av, 0);
    if (0 != rc) {
        BTL_VERBOSE(("%s failed fi_ep_bind with err=%s",
                        linux_device_name,
                        fi_strerror(-rc)
                        ));
        goto fail;
    }

    /* enable the endpoint for using */
    rc = fi_enable(endpoint);
    if (0 != rc) {
        BTL_VERBOSE(("%s failed fi_enable with err=%s",
                        linux_device_name,
                        fi_strerror(-rc)
                        ));
        goto fail;
    }

    /* Everything succeeded, lets create a module for this device. */
    module = (mca_btl_ofi_module_t*) malloc(sizeof(mca_btl_ofi_module_t));
    if (NULL == module) {
        goto fail;
    }
    *module = mca_btl_ofi_module_template;

    /* store the information. */
    module->fabric_info = ofi_info;
    module->fabric = fabric;
    module->domain = domain;
    module->cq = cq;
    module->av = av;
    module->ofi_endpoint = endpoint;
    module->linux_device_name = linux_device_name;
    module->outstanding_rdma = 0;
    module->use_virt_addr = false;

    if (ofi_info->domain_attr->mr_mode == FI_MR_BASIC ||
        ofi_info->domain_attr->mr_mode & FI_MR_VIRT_ADDR)
        module->use_virt_addr = true;

    /* initialize the rcache */
    mca_btl_ofi_rcache_init(module);

    OBJ_CONSTRUCT(&module->endpoints, opal_list_t);

    /* init free lists */
    OBJ_CONSTRUCT(&module->comp_list, opal_free_list_t);
    rc = opal_free_list_init(&module->comp_list,
                             sizeof(mca_btl_ofi_completion_t),
                             opal_cache_line_size,
                             OBJ_CLASS(mca_btl_ofi_completion_t),
                             0,
                             0,
                             128,
                             -1,
                             128,
                             NULL,
                             0,
                             NULL,
                             NULL,
                             NULL);
    assert(OPAL_SUCCESS == rc);

    /* create and send the modex for this device */
    namelen = sizeof(ep_name);
    rc = fi_getname((fid_t)endpoint, &ep_name[0], &namelen);
    if (0 != rc) {
        BTL_VERBOSE(("%s failed fi_getname with err=%s",
                        linux_device_name,
                        fi_strerror(-rc)
                        ));
        abort();
        goto fail;
    }

    /* post our endpoint name so peer can use it to connect to us */
    OPAL_MODEX_SEND(rc,
                    OPAL_PMIX_GLOBAL,
                    &mca_btl_ofi_component.super.btl_version,
                    &ep_name,
                    namelen);
    mca_btl_ofi_component.namelen = namelen;

    /* add this module to the list */
    mca_btl_ofi_component.modules[(*module_count)++] = module;

    return OPAL_SUCCESS;

fail:
    /* clean up */
    fi_close(&av->fid);
    fi_close(&cq->fid);
    fi_close(&endpoint->fid);
    fi_close(&domain->fid);
    fi_close(&fabric->fid);

    /* not really a failure. just skip this device. */
    return OPAL_ERR_OUT_OF_RESOURCE;
}


/**
 * @brief OFI BTL progress function
 *
 * This function explictly progresses all workers.
 */
static int mca_btl_ofi_component_progress (void)
{

    int ret = 0;
    int events_read;
    int events = 0;
    struct fi_cq_entry cq_entry[MCA_BTL_OFI_MAX_CQ_READ_ENTRIES];
    struct fi_cq_err_entry cqerr = {0};

    mca_btl_ofi_completion_t *comp;

    for (int i = 0 ; i < mca_btl_ofi_component.module_count ; ++i) {
        mca_btl_ofi_module_t *module = mca_btl_ofi_component.modules[i];

        ret = fi_cq_read(module->cq, &cq_entry, mca_btl_ofi_component.num_cqe_read);

        if (0 < ret) {
            events_read = ret;
            for (int j = 0; j < events_read; j++) {
                if (NULL != cq_entry[j].op_context) {
                    ++events;
                    comp = (mca_btl_ofi_completion_t*) cq_entry[j].op_context;
                    mca_btl_ofi_module_t *ofi_btl = (mca_btl_ofi_module_t*)comp->btl;

                    switch (comp->type) {
                    case MCA_BTL_OFI_TYPE_GET:
                    case MCA_BTL_OFI_TYPE_PUT:
                    case MCA_BTL_OFI_TYPE_AOP:
                    case MCA_BTL_OFI_TYPE_AFOP:
                    case MCA_BTL_OFI_TYPE_CSWAP:

                        /* call the callback */
                        if (comp->cbfunc) {
                            comp->cbfunc (comp->btl, comp->endpoint,
                                             comp->local_address, comp->local_handle,
                                             comp->cbcontext, comp->cbdata, OPAL_SUCCESS);
                        }

                        /* return the completion handler */
                        opal_free_list_return(comp->my_list, (opal_free_list_item_t*) comp);
                        OPAL_THREAD_ADD_FETCH64(&ofi_btl->outstanding_rdma, -1);
                        break;

                    default:
                        /* catasthrophic */
                        BTL_ERROR(("unknown completion type"));
                        abort();
                    }
                }
            }
        } else if (OPAL_UNLIKELY(ret == -FI_EAVAIL)) {
            ret = fi_cq_readerr(module->cq, &cqerr, 0);

            /* cq readerr failed!? */
            if (0 > ret) {
                BTL_ERROR(("%s:%d: Error returned from fi_cq_readerr: %s(%d)",
                           __FILE__, __LINE__, fi_strerror(-ret), ret));
                fflush(stderr);
                abort();
            }

            BTL_ERROR(("fi_cq_read returned error: (provider err_code = %d)\n"
                        "OFI_BTL will now abort()",
                        cqerr.prov_errno));
            abort();
        }
    }

    return events;
}

/** OFI btl component */
mca_btl_ofi_component_t mca_btl_ofi_component = {
    .super = {
        .btl_version = {
            MCA_BTL_DEFAULT_VERSION("ofi"),
            .mca_open_component = mca_btl_ofi_component_open,
            .mca_close_component = mca_btl_ofi_component_close,
            .mca_register_component_params = mca_btl_ofi_component_register,
        },
        .btl_data = {
            /* The component is not checkpoint ready */
            .param_field = MCA_BASE_METADATA_PARAM_NONE
        },

        .btl_init = mca_btl_ofi_component_init,
        .btl_progress = mca_btl_ofi_component_progress,
    }
};
