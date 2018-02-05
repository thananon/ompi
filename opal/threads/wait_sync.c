/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2014-2016 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2016      Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2017      IBM Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "wait_sync.h"

static opal_mutex_t wait_sync_lock = OPAL_MUTEX_STATIC_INIT;
static ompi_wait_sync_t* wait_sync_list = NULL;

#define WAIT_SYNC_PASS_OWNERSHIP(who)                  \
    do {                                               \
    } while(0)

int ompi_sync_wait_mt(ompi_wait_sync_t *sync)
{
    /* Don't stop if the waiting synchronization is completed. We avoid the
     * race condition around the release of the synchronization using the
     * signaling field.
     */
    if(sync->count <= 0)
        return (0 == sync->status) ? OPAL_SUCCESS : OPAL_ERROR;

    /* Insert sync on the list of pending synchronization constructs */
    OPAL_THREAD_LOCK(&wait_sync_lock);
    if( NULL == wait_sync_list ) {
        sync->next = sync->prev = sync;
        wait_sync_list = sync;
    } else {
        sync->prev = wait_sync_list->prev;
        sync->prev->next = sync;
        sync->next = wait_sync_list;
        wait_sync_list->prev = sync;
    }
    OPAL_THREAD_UNLOCK(&wait_sync_lock);

    /**
     * If we are not responsible for progresing, go silent until something worth noticing happen:
     *  - this thread has been promoted to take care of the progress
     *  - our sync has been triggered.
     */
 check_status:
    while( sync != wait_sync_list ) {
        opal_progress();

        if( sync->count <= 0 ) {
            goto i_am_done;
        }
    }

    while(sync->count > 0) {  /* progress till completion */
        opal_progress();  /* don't progress with the sync lock locked or you'll deadlock */
    }
    assert(sync == wait_sync_list);

 i_am_done:
    /* My sync is now complete. Trim the list: remove self, wake next */
    OPAL_THREAD_LOCK(&wait_sync_lock);
    sync->prev->next = sync->next;
    sync->next->prev = sync->prev;
    /* In case I am the progress manager, pass the duties on */
    if( sync == wait_sync_list ) {
        wait_sync_list = (sync == sync->next) ? NULL : sync->next;
        if( NULL != wait_sync_list )
            WAIT_SYNC_PASS_OWNERSHIP(wait_sync_list);
    }
    OPAL_THREAD_UNLOCK(&wait_sync_lock);

    return (0 == sync->status) ? OPAL_SUCCESS : OPAL_ERROR;
}
