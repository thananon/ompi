/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2018      Intel, Inc, All rights reserved
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "btl_ofi.h"

#define MCA_BTL_OFI_REQUIRED_CAPS       (FI_RMA | FI_ATOMIC)
#define MCA_BTL_OFI_REQUESTED_MR_MODE   (FI_MR_VIRT_ADDR)

/* handle provider specific hints */
static void provider_hints_handler(struct fi_info *hints);


/* info validation after fi_getinfo() */
static int validate_info(struct fi_info *info);
static int socks_validate_info(struct fi_info *info);
static int psm2_validate_info(struct fi_info *info);


/* functions below */

static int socks_validate_info(struct fi_info *info)
{
    /* placeholder */
    return OPAL_SUCCESS;
}

static int psm2_validate_info(struct fi_info *info)
{
    /* placeholder */
    return OPAL_SUCCESS;
}

/* validate information returned from fi_getinfo().
 * return OPAL_ERROR if we dont have what we need. */
static int validate_info(struct fi_info *info)
{
    int *mr_mode;
    char *prov_name = info->fabric_attr->prov_name;

    /* we need exactly all the required bits */
    if ((info->caps & MCA_BTL_OFI_REQUIRED_CAPS) != MCA_BTL_OFI_REQUIRED_CAPS)
        return OPAL_ERROR;

    /* we need FI_EP_RDM */
    if (info->ep_attr->type != FI_EP_RDM)
        return OPAL_ERROR;

    mr_mode = &info->domain_attr->mr_mode;

    if (*mr_mode == FI_MR_UNSPEC) {
        /* If ofi returns FI_MR_UNSPEC, it means the provider
         * support both FI_MR_SCALABLE and FI_MR_BASIC.
         * Hence, we will force FI_MR_BASIC */
        *mr_mode = FI_MR_BASIC;

    } else if ((*mr_mode & MCA_BTL_OFI_REQUESTED_MR_MODE) !=
                           MCA_BTL_OFI_REQUESTED_MR_MODE) {
        return OPAL_ERROR;
    } else if (*mr_mode & FI_MR_LOCAL) {
        /* ofi btl does not register local memory. */
        return OPAL_ERROR;
    }

    /* provider specifics */
    if (!strcmp(prov_name, "sockets")) {
        return socks_validate_info(info);
    } else if (!strcmp(prov_name, "psm2")) {
        return psm2_validate_info(info);
    }

    return OPAL_SUCCESS;
}

/* fill out specific hints needed for each provider */
static void provider_hints_handler(struct fi_info *hints)
{
    /* placeholder for providers to modify their hints */
    //char *prov_name = hints->fabric_attr->prov_name;
}
