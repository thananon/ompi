/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef BTL_USNIC_RECV_H
#define BTL_USNIC_RECV_H

#include "btl_usnic.h"
#include "btl_usnic_util.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_proc.h"


void opal_btl_usnic_recv_call(opal_btl_usnic_module_t *module,
                              opal_btl_usnic_recv_segment_t *rseg,
                              opal_btl_usnic_channel_t *channel);

static inline int
opal_btl_usnic_post_recv_list(opal_btl_usnic_channel_t *channel)
{
    struct iovec iov;
    struct fi_msg msg;
    uint64_t flag = 0;
    opal_btl_usnic_recv_segment_t *rseg;
    int rc = OPAL_SUCCESS;

    msg.msg_iov = &iov;
    msg.iov_count = 1;
    for (rseg = channel->repost_recv_head; NULL != rseg; rseg = rseg->rs_next) {
        msg.context = rseg;
        iov.iov_base = rseg->rs_protocol_header;
        iov.iov_len = rseg->rs_len;

        ++channel->rx_post_cnt;

        rc = fi_recvmsg(channel->ep, &msg, flag);
        if (0 != rc) {
            return rc;
        }
    }
    channel->repost_recv_head = NULL;

    return 0;
}

/*
 * Given an incoming segment, lookup the endpoint that sent it
 */
static inline opal_btl_usnic_endpoint_t *
lookup_sender(opal_btl_usnic_module_t *module, opal_btl_usnic_segment_t *seg)
{
    int ret;
    opal_btl_usnic_endpoint_t *sender;

    /* Use the hashed ORTE process name in the BTL header to uniquely
       identify the sending process (using the MAC/hardware address
       only identifies the sending server -- not the sending ORTE
       process). */
    /* JMS We've experimented with using a handshake before sending
       any data so that instead of looking up a hash on the
       btl_header->sender, echo back the ptr to the sender's
       ompi_proc.  There was limited speedup with this scheme; more
       investigation is required. */
    ret = opal_hash_table_get_value_uint64(&module->senders,
                                           seg->us_btl_header->sender,
                                           (void**) &sender);
    if (OPAL_LIKELY(OPAL_SUCCESS == ret)) {
        return sender;
    }

    /* The sender wasn't in the hash table, so do a slow lookup and
       put the result in the hash table */
    sender = opal_btl_usnic_proc_lookup_endpoint(module,
                                                 seg->us_btl_header->sender);
    if (NULL != sender) {
        opal_hash_table_set_value_uint64(&module->senders,
                                         seg->us_btl_header->sender, sender);
        return sender;
    }

    /* Whoa -- not found at all! */
    return NULL;
}

/*
 * We have received a segment, take action based on the
 * packet type in the BTL header.
 * Try to be fast here - defer as much bookkeeping until later as
 * possible.
 * See README.txt for a discussion of receive fastpath
 */
static inline void
opal_btl_usnic_recv_fast(opal_btl_usnic_module_t *module,
                         opal_btl_usnic_recv_segment_t *seg,
                         opal_btl_usnic_channel_t *channel)
{
    opal_btl_usnic_segment_t *bseg;
    mca_btl_active_message_callback_t* reg;
    opal_btl_usnic_endpoint_t *endpoint;

    /* Make the whole payload Valgrind defined */
    opal_memchecker_base_mem_defined(seg->rs_protocol_header, seg->rs_len);

    bseg = &seg->rs_base;

    /* Find out who sent this segment */
    endpoint = lookup_sender(module, bseg);
    seg->rs_endpoint = endpoint;

#if 0
opal_output(0, "fast recv %d bytes:\n", bseg->us_btl_header->payload_len + sizeof(opal_btl_usnic_btl_header_t));
opal_btl_usnic_dump_hex(bseg->us_btl_header, bseg->us_btl_header->payload_len + sizeof(opal_btl_usnic_btl_header_t));
#endif
    /* If this is a short incoming message (i.e., the message is
       wholly contained in this one message -- it is not chunked
       across multiple messages), and it's not a PUT from the sender,
       then just handle it here. */
    if (endpoint != NULL && !endpoint->endpoint_exiting &&
            (OPAL_BTL_USNIC_PAYLOAD_TYPE_FRAG ==
                bseg->us_btl_header->payload_type) &&
            seg->rs_base.us_btl_header->put_addr == NULL) {

        /* Pass this segment up to the PML.
         * Be sure to get the payload length from the BTL header because
         * the L2 layer may artificially inflate (or otherwise change)
         * the frame length to meet minimum sizes, add protocol information,
         * etc.
         */
        reg = mca_btl_base_active_message_trigger + bseg->us_btl_header->tag;
        seg->rs_segment.seg_len = bseg->us_btl_header->payload_len;
        reg->cbfunc(&module->super, bseg->us_btl_header->tag,
                    &seg->rs_desc, reg->cbdata);
        channel->chan_deferred_recv = seg;
    }

    /* Otherwise, handle all the other cases the "normal" way */
    else {
        opal_btl_usnic_recv_call(module, seg, channel);
    }
}

/*
 */
static inline int
opal_btl_usnic_recv_frag_bookkeeping(
    opal_btl_usnic_module_t* module,
    opal_btl_usnic_recv_segment_t *seg,
    opal_btl_usnic_channel_t *channel)
{
    opal_btl_usnic_endpoint_t* endpoint;

    endpoint = seg->rs_endpoint;

    /* Valgrind help */
    opal_memchecker_base_mem_defined(
                (void*)(seg->rs_protocol_header), seg->rs_len);

    ++module->stats.num_total_recvs;
    ++module->stats.num_frag_recvs;

    /* if endpoint exiting, and all ACKs received, release the endpoint */
    if (endpoint->endpoint_exiting && ENDPOINT_DRAINED(endpoint)) {
        OBJ_RELEASE(endpoint);
    }

    /* Add recv to linked list for reposting */
    seg->rs_next = channel->repost_recv_head;
    channel->repost_recv_head = seg;

    return OPAL_SUCCESS;
}

/*
 * We have received a segment, take action based on the
 * packet type in the BTL header
 */
static inline void
opal_btl_usnic_recv(opal_btl_usnic_module_t *module,
                    opal_btl_usnic_recv_segment_t *seg,
                    opal_btl_usnic_channel_t *channel)
{
    opal_btl_usnic_segment_t *bseg;
    mca_btl_active_message_callback_t* reg;
    opal_btl_usnic_endpoint_t *endpoint;
    int rc;

    /* Make the whole payload Valgrind defined */
    opal_memchecker_base_mem_defined(seg->rs_protocol_header, seg->rs_len);

    bseg = &seg->rs_base;

    /* Find out who sent this segment */
    endpoint = lookup_sender(module, bseg);
    seg->rs_endpoint = endpoint;

    /* If this is a short incoming message (i.e., the message is
       wholly contained in this one message -- it is not chunked
       across multiple messages), and it's not a PUT from the sender,
       then just handle it here. */
    if (endpoint != NULL && !endpoint->endpoint_exiting &&
            (OPAL_BTL_USNIC_PAYLOAD_TYPE_FRAG ==
                bseg->us_btl_header->payload_type) &&
            seg->rs_base.us_btl_header->put_addr == NULL) {

        MSGDEBUG1_OUT("<-- Received FRAG (fastpath) ep %p, seq %" UDSEQ ", len=%" PRIu16 "\n",
                      (void*) endpoint, bseg->us_btl_header->pkt_seq,
                      bseg->us_btl_header->payload_len);

        /* do the receive bookkeeping */
        rc = opal_btl_usnic_recv_frag_bookkeeping(module, seg, channel);
        if (rc != 0) {
            return;
        }

        /* Pass this segment up to the PML.
         * Be sure to get the payload length from the BTL header because
         * the L2 layer may artificially inflate (or otherwise change)
         * the frame length to meet minimum sizes, add protocol information,
         * etc.
         */
        reg = mca_btl_base_active_message_trigger + bseg->us_btl_header->tag;
        seg->rs_segment.seg_len = bseg->us_btl_header->payload_len;
        reg->cbfunc(&module->super, bseg->us_btl_header->tag,
                    &seg->rs_desc, reg->cbdata);

    }

    /* Otherwise, handle all the other cases the "normal" way */
    else {
        opal_btl_usnic_recv_call(module, seg, channel);
    }
}

#endif /* BTL_USNIC_RECV_H */
