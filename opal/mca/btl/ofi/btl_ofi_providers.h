/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2018      Intel, Inc, All rights reserved
 * Copyright (c) 2018      Los Alamos National Security, LLC. All rights
 *                         reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "btl_ofi.h"

#define MCA_BTL_OFI_REQUIRED_CAPS       (FI_RMA | FI_ATOMIC)
#define MCA_BTL_OFI_REQUESTED_MR_MODE   (FI_MR_SCALABLE)

/* handle provider specific hints */
static void provider_hints_handler(struct fi_info *hints);


/* info validation after fi_getinfo() */
static int validate_info(struct fi_info *info);
static int socks_validate_info(struct fi_info *info);
static int psm2_validate_info(struct fi_info *info);
static int gni_validate_info(struct fi_info *info);


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

static int gni_validate_info(struct fi_info *info)
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

    if (*mr_mode & FI_MR_LOCAL) {
        /* ofi btl does not register local memory. */
        return OPAL_ERROR;
    }

    /* provider specifics */
    if (!strcmp(prov_name, "sockets")) {
        return socks_validate_info(info);
    } else if (!strcmp(prov_name, "psm2")) {
        return psm2_validate_info(info);
    } else if (!strcmp(prov_name, "gni")) {
        return gni_validate_info(info);
    }

    return OPAL_SUCCESS;
}

/* fill out specific hints needed for each provider */
static void provider_hints_handler(struct fi_info *hints)
{
    char *prov_name;

    assert(hints->fabric_attr);
    assert(hints->domain_attr);
    assert(hints->fabric_attr->prov_name);

    prov_name = hints->fabric_attr->prov_name;

    if (!strcmp(prov_name, "gni")) {
        hints->domain_attr->mr_mode = FI_MR_BASIC;
    }
}
