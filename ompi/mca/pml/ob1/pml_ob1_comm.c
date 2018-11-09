/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2018 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 *
 * Copyright (c) 2018      Sandia National Laboratories
 * 			   All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"
#include <string.h>

#include "pml_ob1.h"
#include "pml_ob1_comm.h"

#include "opal/class/opal_task.h"

static void mca_pml_ob1_comm_proc_construct(mca_pml_ob1_comm_proc_t* proc)
{
    proc->ompi_proc = NULL;
    proc->expected_sequence = 1;
    proc->send_sequence = 0;
    proc->frags_cant_match = NULL;
#if !MCA_PML_OB1_CUSTOM_MATCH
    OBJ_CONSTRUCT(&proc->specific_receives, opal_list_t);
    OBJ_CONSTRUCT(&proc->unexpected_frags, opal_list_t);
#endif
}


static void mca_pml_ob1_comm_proc_destruct(mca_pml_ob1_comm_proc_t* proc)
{
    assert(NULL == proc->frags_cant_match);
#if !MCA_PML_OB1_CUSTOM_MATCH
    OBJ_DESTRUCT(&proc->specific_receives);
    OBJ_DESTRUCT(&proc->unexpected_frags);
#endif
    if (proc->ompi_proc) {
        OBJ_RELEASE(proc->ompi_proc);
    }
}


OBJ_CLASS_INSTANCE(mca_pml_ob1_comm_proc_t, opal_object_t,
                   mca_pml_ob1_comm_proc_construct,
                   mca_pml_ob1_comm_proc_destruct);


static void mca_pml_ob1_comm_construct(mca_pml_ob1_comm_t* comm)
{
#if !MCA_PML_OB1_CUSTOM_MATCH
    OBJ_CONSTRUCT(&comm->wild_receives, opal_list_t);
#else
    comm->prq = custom_match_prq_init();
    comm->umq = custom_match_umq_init();
#endif
    OBJ_CONSTRUCT(&comm->matching_lock, opal_mutex_t);
    OBJ_CONSTRUCT(&comm->proc_lock, opal_mutex_t);

    OBJ_CONSTRUCT(&comm->taskpool, opal_free_list_t);
    (void) opal_free_list_init (&comm->taskpool, sizeof (opal_task_t),
                                opal_cache_line_size, OBJ_CLASS(opal_task_t),
                                0, opal_cache_line_size, 512, -1, 512, NULL, 0,
                                NULL, NULL, NULL);

    comm->recv_sequence = 0;
    comm->procs = NULL;
    comm->last_probed = 0;
    comm->num_procs = 0;
}


static void mca_pml_ob1_comm_destruct(mca_pml_ob1_comm_t* comm)
{
    if (NULL != comm->procs) {
        for (size_t i = 0; i < comm->num_procs; ++i) {
            if (comm->procs[i]) {
                OBJ_RELEASE(comm->procs[i]);
            }
        }

        free(comm->procs);
    }

#if !MCA_PML_OB1_CUSTOM_MATCH
    OBJ_DESTRUCT(&comm->wild_receives);
#else
    custom_match_prq_destroy(comm->prq);
    custom_match_umq_destroy(comm->umq);
#endif
    OBJ_DESTRUCT(&comm->matching_lock);
    OBJ_DESTRUCT(&comm->proc_lock);
}


OBJ_CLASS_INSTANCE(
    mca_pml_ob1_comm_t,
    opal_object_t,
    mca_pml_ob1_comm_construct,
    mca_pml_ob1_comm_destruct);


int mca_pml_ob1_comm_init_size (mca_pml_ob1_comm_t* comm, size_t size)
{
    /* send message sequence-number support - sender side */
    comm->procs = (mca_pml_ob1_comm_proc_t **) calloc(size, sizeof (mca_pml_ob1_comm_proc_t *));
    if(NULL == comm->procs) {
        return OMPI_ERR_OUT_OF_RESOURCE;
    }
    comm->num_procs = size;
    return OMPI_SUCCESS;
}


