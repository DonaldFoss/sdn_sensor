#include <stdint.h>
#include <stdio.h>

#include <netinet/icmp6.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>

#include <rte_byteorder.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>

#include "checksum.h"
#include "common.h"
#include "ethernet.h"
#include "icmp.h"
#include "l4_utils.h"
#include "sdn_sensor.h"

// ICMPv6 Pseudo Header
// 
// Source Address
// Destination Address
// 
// ICMPv6 Length (32 bits)
// Zeros         (24 bits)
// Next Header   ( 8 bits)
int ss_frame_prepare_icmp6(ss_frame_t* tx_buf, uint8_t* pl_ptr, uint16_t pl_len) {
    rte_mbuf_t* pmbuf = NULL;
    uint8_t* pptr;
    uint32_t icmp_len;
    uint32_t zeros_nxt;
    uint16_t checksum;

    pmbuf = rte_pktmbuf_alloc(ss_pool[rte_socket_id()]);
    if (pmbuf == NULL) {
        RTE_LOG(ERR, L3L4, "could not allocate mbuf icmp6 pseudo header\n");
        goto error_out;
    }
    icmp_len  = rte_bswap32(pl_len);
    zeros_nxt = rte_bswap32((uint32_t) tx_buf->ip6->ip6_nxt);

    /* XXX: switch to ss_phdr_append */
    pptr = ss_phdr_append(pmbuf, &tx_buf->ip6->ip6_src, sizeof(tx_buf->ip6->ip6_src));
    if (pptr == NULL) goto error_out;
    pptr = ss_phdr_append(pmbuf, &tx_buf->ip6->ip6_dst, sizeof(tx_buf->ip6->ip6_dst));
    if (pptr == NULL) goto error_out;
    pptr = ss_phdr_append(pmbuf, &icmp_len, sizeof(icmp_len));
    if (pptr == NULL) goto error_out;
    pptr = ss_phdr_append(pmbuf, &zeros_nxt, sizeof(zeros_nxt));
    if (pptr == NULL) goto error_out;
    pptr = ss_phdr_append(pmbuf, pl_ptr, pl_len);
    if (pptr == NULL) goto error_out;

    RTE_LOG(FINE, L3L4, "icmp6 tx size %u\n", pl_len);
    if (rte_get_log_level() >= RTE_LOG_FINER) {
        printf("icmp6 pseudo-header:\n");
        rte_pktmbuf_dump(stderr, pmbuf, rte_pktmbuf_pkt_len(pmbuf));
    }
    checksum = ss_in_cksum(rte_pktmbuf_mtod(pmbuf, uint16_t*), rte_pktmbuf_pkt_len(pmbuf));
    rte_pktmbuf_free(pmbuf);
    tx_buf->icmp6->icmp6_cksum = checksum;
    //tx_buf->ndp_tx->hdr.nd_na_cksum = checksum;

    return 0;

    error_out:
    if (tx_buf->mbuf) {
        RTE_LOG(ERR, L3L4, "could not process icmp6 frame\n");
        rte_pktmbuf_free(pmbuf);
        tx_buf->active = 0;
        rte_pktmbuf_free(tx_buf->mbuf);
        tx_buf->mbuf = NULL;
    }
    return -1;
}

int ss_frame_handle_echo4(ss_frame_t* rx_buf, ss_frame_t* tx_buf) {
    int rv = 0;
    uint16_t checksum;
    uint8_t* dptr;

    rv = ss_frame_prepare_eth(tx_buf, rx_buf->data.port_id, (eth_addr_t*) &rx_buf->eth->s_addr, ETHER_TYPE_IPV4);
    if (rv) {
        RTE_LOG(ERR, L3L4, "could not prepare ethernet mbuf\n");
        goto error_out;
    }
    
    ss_frame_prepare_ip4(rx_buf, tx_buf);

    tx_buf->icmp4 = (icmp4_hdr_t*) rte_pktmbuf_append(tx_buf->mbuf, sizeof(icmp4_hdr_t));
    if (tx_buf->icmp4 == NULL) {
        RTE_LOG(ERR, L3L4, "could not allocate mbuf icmp4 header\n");
        goto error_out;
    }
    tx_buf->icmp4->type              = ICMP_ECHOREPLY;
    tx_buf->icmp4->code              = 0;
    tx_buf->icmp4->checksum          = rte_bswap16(0x0000);
    tx_buf->icmp4->un.echo.id        = rx_buf->icmp4->un.echo.id;
    tx_buf->icmp4->un.echo.sequence  = rx_buf->icmp4->un.echo.sequence;
    dptr = (uint8_t*) rte_pktmbuf_append(tx_buf->mbuf, rte_bswap16(rx_buf->ip4->tot_len) - sizeof(ip4_hdr_t) - sizeof(icmp4_hdr_t));
    if (dptr == NULL) {
        RTE_LOG(ERR, L3L4, "could not allocate mbuf icmp4 dptr\n");
        goto error_out;
    }
    rte_memcpy(dptr, (uint8_t*) rx_buf->icmp4 + sizeof(icmp4_hdr_t), rx_buf->ip4->tot_len - sizeof(ip4_hdr_t) - sizeof(icmp4_hdr_t));

    checksum = ss_in_cksum((uint16_t*) tx_buf->icmp4, (uint16_t) (rte_pktmbuf_pkt_len(tx_buf->mbuf) - ((uint8_t*) tx_buf->icmp4 - rte_pktmbuf_mtod(tx_buf->mbuf, uint8_t*))));
    tx_buf->icmp4->checksum          = checksum;

    tx_buf->ip4->tot_len             = rte_bswap16(rte_pktmbuf_pkt_len(tx_buf->mbuf) - sizeof(eth_hdr_t)); // XXX: better way?
    checksum = ss_in_cksum((uint16_t*) tx_buf->ip4, sizeof(ip4_hdr_t));
    tx_buf->ip4->check               = checksum;

    return 0;

    error_out:
    if (tx_buf->mbuf) {
        RTE_LOG(ERR, L3L4, "could not process icmp4 frame\n");
        tx_buf->active = 0;
        rte_pktmbuf_free(tx_buf->mbuf);
        tx_buf->mbuf = NULL;
    }
    return -1;
}

int ss_frame_handle_echo6(ss_frame_t* rx_buf, ss_frame_t* tx_buf) {
    int rv = 0;
    uint8_t* dptr;
    uint16_t rx_dlen;
    uint16_t tx_plen;

    rv = ss_frame_prepare_eth(tx_buf, rx_buf->data.port_id, (eth_addr_t*) &rx_buf->eth->s_addr, ETHER_TYPE_IPV6);
    if (rv) {
        RTE_LOG(ERR, L3L4, "could not prepare ethernet mbuf\n");
        goto error_out;
    }
    
    ss_frame_prepare_ip6(tx_buf, rx_buf);

    tx_buf->icmp6 = (icmp6_hdr_t*) rte_pktmbuf_append(tx_buf->mbuf, sizeof(icmp6_hdr_t));
    if (tx_buf->icmp6 == NULL) {
        RTE_LOG(ERR, L3L4, "could not allocate mbuf icmp6 header\n");
        goto error_out;
    }
    tx_buf->icmp6->icmp6_type        = ICMP6_ECHO_REPLY;
    tx_buf->icmp6->icmp6_code        = 0;
    tx_buf->icmp6->icmp6_cksum       = rte_bswap16(0x0000);
    tx_buf->icmp6->icmp6_data16[0]   = rx_buf->icmp6->icmp6_data16[0]; // ICMP ID
    tx_buf->icmp6->icmp6_data16[1]   = rx_buf->icmp6->icmp6_data16[1]; // Sequence Number
    rx_dlen                          = rte_bswap16(rx_buf->ip6->ip6_plen) - sizeof(icmp6_hdr_t);
    dptr = (uint8_t*) rte_pktmbuf_append(tx_buf->mbuf, rx_dlen);
    if (dptr == NULL) {
        RTE_LOG(ERR, L3L4, "could not allocate mbuf icmp6 dptr\n");
        goto error_out;
    }
    rte_memcpy(dptr, (uint8_t*) rx_buf->icmp6 + sizeof(icmp6_hdr_t), rx_dlen);
    tx_plen                          = (uint16_t) (rte_pktmbuf_pkt_len(tx_buf->mbuf) - sizeof(eth_hdr_t) - sizeof(ip6_hdr_t)); // XXX: better way?
    tx_buf->ip6->ip6_plen            = rte_bswap16(tx_plen);

    rv = ss_frame_prepare_icmp6(tx_buf, (uint8_t*) tx_buf->icmp6, tx_plen);
    if (rv) {
        RTE_LOG(ERR, L3L4, "could not prepare echo6 frame\n");
        goto error_out;
    }
    // mhall
    if (rte_get_log_level() >= RTE_LOG_DEBUG) {
        RTE_LOG(DEBUG, L3L4, "debug echo6\n");
        rte_pktmbuf_dump(stderr, tx_buf->mbuf, rte_pktmbuf_pkt_len(tx_buf->mbuf));
    }

    return 0;

    error_out:
    if (tx_buf->mbuf) {
        RTE_LOG(ERR, L3L4, "could not process icmp6 frame\n");
        tx_buf->active = 0;
        rte_pktmbuf_free(tx_buf->mbuf);
        tx_buf->mbuf = NULL;
    }
    return -1;
}

int ss_frame_handle_icmp4(ss_frame_t* rx_buf, ss_frame_t* tx_buf) {
    int rv = 0;

    //rx_buf->icmp4 = (icmp4_hdr_t*) ((uint8_t*) rx_buf->ip4 + sizeof(ip4_hdr_t));

    uint8_t icmp_type = rx_buf->icmp4->type;
    uint8_t icmp_code = rx_buf->icmp4->code;
    ss_frame_layer_off_len_get(rx_buf, rx_buf->icmp4, sizeof(icmp4_hdr_t), &rx_buf->l4_offset, &rx_buf->data.l4_length);
    rx_buf->data.icmp_type = icmp_type;
    rx_buf->data.icmp_code = icmp_code;
    RTE_LOG(FINE, L3L4, "icmp4 type %hhu\n", icmp_type);
    switch (icmp_type) {
        case ICMP_ECHO: {
            RTE_LOG(DEBUG, L3L4, "rx icmp echo packet\n");
            SS_CHECK_SELF(rx_buf, 0);
            rv = ss_frame_handle_echo4(rx_buf, tx_buf);
            break;
        }
        default: {
            //RTE_LOG(INFO, L3L4, "port %u received unsupported icmpv4 0x%04hhx frame:\n", rx_buf->data.port_id, icmp_type);
            //rte_pktmbuf_dump(stderr, rx_buf->mbuf, rte_pktmbuf_pkt_len(rx_buf->mbuf));
            rv = -1;
            break;
        }
    }

    return rv;
}

int ss_frame_handle_icmp6(ss_frame_t* rx_buf, ss_frame_t* tx_buf) {
    int rv = 0;

    //rx_buf->icmp6 = (icmp6_hdr_t*) ((uint8_t*) rx_buf->ip6 + sizeof(ip6_hdr_t));

    // XXX: add the PMTUD request
    uint8_t icmp_type = rx_buf->icmp6->icmp6_type;
    uint8_t icmp_code = rx_buf->icmp6->icmp6_code;
    ss_frame_layer_off_len_get(rx_buf, rx_buf->icmp6, sizeof(icmp6_hdr_t), &rx_buf->l4_offset, &rx_buf->data.l4_length);
    rx_buf->data.icmp_type = icmp_type;
    rx_buf->data.icmp_code = icmp_code;
    RTE_LOG(FINE, L3L4, "icmp6 type %hhu\n", icmp_type);
    switch (icmp_type) {
        case ICMP6_ECHO_REQUEST: {
            RTE_LOG(DEBUG, L3L4, "rx icmp6 echo packet\n");
            rv = ss_frame_handle_echo6(rx_buf, tx_buf);
            break;
        }
        case ND_NEIGHBOR_SOLICIT: {
            RTE_LOG(DEBUG, L3L4, "rx icmp6 ndp packet\n");
            rv = ss_frame_handle_ndp(rx_buf, tx_buf);
            break;
        }
        default: {
            RTE_LOG(INFO, STACK, "port %u received unsupported icmpv6 0x%04hhx frame:\n", rx_buf->data.port_id, icmp_type);
            //rte_pktmbuf_dump(stderr, rx_buf->mbuf, rte_pktmbuf_pkt_len(rx_buf->mbuf));
            rv = -1;
            break;
        }
    }

    return rv;
}
