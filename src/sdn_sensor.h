#ifndef __SDN_SENSOR_H__
#define __SDN_SENSOR_H__

#define RTE_LOGTYPE_SS RTE_LOGTYPE_USER1

#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define NB_MBUF   8192

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16

#define TIMER_MILLISECOND 2000000ULL /* around 1ms at 2 Ghz */
#define MAX_TIMER_PERIOD 86400 /* 1 day max */

struct mbuf_table {
    unsigned len;
    struct rte_mbuf* m_table[MAX_PKT_BURST];
};

struct lcore_queue_conf {
    unsigned n_rx_port;
    unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
    struct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];

} __rte_cache_aligned;

struct ss_port_statistics {
    uint64_t tx;
    uint64_t rx;
    uint64_t dropped;
} __rte_cache_aligned;

/* BEGIN PROTOTYPES */

int ss_send_burst(struct lcore_queue_conf* qconf, unsigned int n, uint8_t port);
int ss_send_packet(struct rte_mbuf* m, uint8_t port);
void ss_simple_forward(struct rte_mbuf* m, unsigned int portid);
void ss_main_loop(void);
int ss_launch_one_lcore(void* dummy);
void ss_usage(const char* prgname);
int ss_parse_args(int argc, char* *argv);
int main(int argc, char* argv[]);

/* END PROTOTYPES */

#endif /* __SDN_SENSOR_H__ */