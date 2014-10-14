#define _GNU_SOURCE /* strcasestr */

#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>

#include <bsd/string.h>
#include <bsd/sys/queue.h>

#include <jemalloc/jemalloc.h>

#include <json-c/json.h>
#include <json-c/json_object_private.h>

#include <rte_log.h>

#include "common.h"
#include "ip_utils.h"
#include "sdn_sensor.h"
#include "sensor_conf.h"

#define PROGRAM_PATH "/proc/self/exe"
#define CONF_PATH "/../conf/sdn_sensor.json"

int ss_conf_destroy() {
    // XXX: destroy everything else in ss_conf_t
    je_free(ss_conf);
    return 0;
}

char* ss_conf_path_get() {
    size_t path_size = PATH_MAX;
    char* program_directory = NULL;
    
    char* program_path = je_calloc(1, path_size);
    if (program_path == NULL) {
        goto error_out;
    }
    
    ssize_t rv = readlink(PROGRAM_PATH, program_path, path_size);
    if (rv < 0) {
        goto error_out;
    }
    
    char* directory = dirname(program_path);
    
    program_directory = je_calloc(1, path_size);
    if (program_directory == NULL) {
        goto error_out;
    }
    
    strlcpy(program_directory, directory, path_size);
    strlcat(program_directory, CONF_PATH, path_size);
    
    error_out:
    if (program_path) je_free(program_path);
    
    return program_directory;
}

char* ss_conf_file_read() {
    int is_ok = 1;
    int rv;
    size_t srv;
    
    char* conf_path = ss_conf_path_get();
    char* conf_content = NULL;
    
    FILE* conf_file = fopen(conf_path, "rb");
    if (conf_file == NULL) {
        is_ok = 0;
        fprintf(stderr, "error: could not open configuration file %s\n", conf_path);
        goto error_out;
    }
    
    rv = fseek(conf_file, 0L, SEEK_END);
    if (rv == -1) {
        is_ok = 0;
        fprintf(stderr, "error: could not seek to end of configuration file\n");
        goto error_out;
    }
    
    size_t size = ftell(conf_file);
    if (size == (unsigned long) -1) {
        is_ok = 0;
        fprintf(stderr, "error: could not get size of configuration file\n");
        goto error_out;
    }
    
    rewind(conf_file);
    
    /* make room for terminating NUL */
    conf_content = je_calloc(1, size + 1);
    if (conf_content == NULL) {
        is_ok = 0;
        fprintf(stderr, "error: could not allocate configuration file buffer\n");
        goto error_out;
    }
    
    srv = fread(conf_content, 1, size, conf_file);
    if (srv != size) {
        is_ok = 0;
        fprintf(stderr, "error: could not load configuration file\n");
        goto error_out;
    }
    
    /* insert terminating NUL */
    conf_content[size - 1] = '\0';
    
    error_out:
    if (conf_path)              { je_free(conf_path);    conf_path    = NULL; }
    if (conf_file)              { fclose(conf_file);     conf_file    = NULL; }
    if (!is_ok && conf_content) { je_free(conf_content); conf_content = NULL; }
    
    return conf_content;
}

int ss_conf_network_parse(json_object* items) {
    int rv;
    json_object* item = NULL;
    
    item = json_object_object_get(items, "promiscuous_mode");
    if (item) {
        if (!json_object_is_type(item, json_type_boolean)) {
            fprintf(stderr, "promiscuous_mode is not boolean\n");
            return -1;
        }
        ss_conf->promiscuous_mode = json_object_get_boolean(item);
    }
    item = json_object_object_get(items, "mtu");
    if (item) {
        if (!json_object_is_type(item, json_type_int)) {
            fprintf(stderr, "mtu is not int\n");
            return -1;
        }
        ss_conf->mtu = json_object_get_int(item);
    }
    item = json_object_object_get(items, "ipv4_address");
    if (item) {
        if (!json_object_is_type(item, json_type_string)) {
            fprintf(stderr, "ipv4_address is not string\n");
            return -1;
        }
        fprintf(stderr, "parse ipv4 address %s\n", json_object_get_string(item));
        rv = ss_cidr_parse(json_object_get_string(item), &ss_conf->ip4_address);
        if (rv != 1) {
            fprintf(stderr, "invalid ipv4 address %s\n", json_object_get_string(item));
            return -1;
        }
        ss_cidr_dump(NULL, "ipv4_address", &ss_conf->ip4_address);
    }
    item = json_object_object_get(items, "ipv4_gateway");
    if (item) {
        if (!json_object_is_type(item, json_type_string)) {
            fprintf(stderr, "ipv4_gateway is not string\n");
            return -1;
        }
        fprintf(stderr, "parse ipv4 gateway %s\n", json_object_get_string(item));
        rv = ss_cidr_parse(json_object_get_string(item), &ss_conf->ip4_gateway);
        if (rv != 1) {
            fprintf(stderr, "invalid ipv4 gateway %s\n", json_object_get_string(item));
            return -1;
        }
        ss_cidr_dump(NULL, "ipv4_gateway", &ss_conf->ip4_gateway);
    }
    item = json_object_object_get(items, "ipv6_address");
    if (item) {
        if (!json_object_is_type(item, json_type_string)) {
            fprintf(stderr, "ipv6_address is not string\n");
            return -1;
        }
        fprintf(stderr, "parse ipv6 address %s\n", json_object_get_string(item));
        rv = ss_cidr_parse(json_object_get_string(item), &ss_conf->ip6_address);
        if (rv != 1) {
            fprintf(stderr, "invalid ipv6 address %s\n", json_object_get_string(item));
            return -1;
        }
        ss_cidr_dump(NULL, "ipv6_address", &ss_conf->ip6_address);
    }
    item = json_object_object_get(items, "ipv6_gateway");
    if (item) {
        if (!json_object_is_type(item, json_type_string)) {
            fprintf(stderr, "ipv6_gateway is not string\n");
            return -1;
        }
        fprintf(stderr, "parse ipv6 gateway %s\n", json_object_get_string(item));
        rv = ss_cidr_parse(json_object_get_string(item), &ss_conf->ip6_gateway);
        if (rv != 1) {
            fprintf(stderr, "invalid ipv6 gateway %s\n", json_object_get_string(item));
            return -1;
        }
        ss_cidr_dump(NULL, "ipv6_gateway", &ss_conf->ip6_gateway);
    }
    return 0;
}

int ss_conf_dpdk_parse(json_object* items) {
    int rv;
    json_object* item = NULL;
    
    item = json_object_object_get(items, "eal_options");
    if (item) {
        if (!json_object_is_type(item, json_type_string)) {
            fprintf(stderr, "eal_options is not string\n");
            return -1;
        }
        fprintf(stderr, "parse eal options %s\n", json_object_get_string(item));
        memset(&ss_conf->eal_vector, 0, sizeof(ss_conf->eal_vector));
        rv = wordexp(json_object_get_string(item), &ss_conf->eal_vector, WRDE_NOCMD);
        if (rv) {
            fprintf(stderr, "could not parse eal options: %d\n", rv);
            return -1;
        }
    }
    
    // XXX: eventually support hex numbers
    item = json_object_object_get(items, "port_mask");
    if (item) {
        if (!json_object_is_type(item, json_type_int)) {
            fprintf(stderr, "port_mask is not integer\n");
            return -1;
        }
        ss_conf->port_mask = json_object_get_int(item);
    }
    else {
        ss_conf->port_mask = 0xFFFFFFFF;
    }
    
    item = json_object_object_get(items, "rxd_count");
    if (item) {
        if (!json_object_is_type(item, json_type_int)) {
            fprintf(stderr, "rxd_count is not integer\n");
            return -1;
        }
        ss_conf->rxd_count = json_object_get_int(item);
    }
    else {
        ss_conf->rxd_count = 128 /* RTE_TEST_RX_DESC_DEFAULT */;
    }
    
    item = json_object_object_get(items, "txd_count");
    if (item) {
        if (!json_object_is_type(item, json_type_int)) {
            fprintf(stderr, "txd_count is not integer\n");
            return -1;
        }
        ss_conf->txd_count = json_object_get_int(item);
    }
    else {
        ss_conf->txd_count = 512 /* RTE_TEST_TX_DESC_DEFAULT */;
    }

    item = json_object_object_get(items, "timer_msec");
    if (item) {
        if (!json_object_is_type(item, json_type_int)) {
            fprintf(stderr, "queue_count is not integer\n");
            return -1;
        }
        ss_conf->timer_msec = json_object_get_int(item);
        if (ss_conf->timer_msec > MAX_TIMER_PERIOD) {
            fprintf(stderr, "timer_msec larger than %d\n", MAX_TIMER_PERIOD);
            return -1;
        }
    }
    else {
        /* A TSC-based timer responsible for triggering statistics printout */
        /* default period is 10 seconds */
        ss_conf->timer_msec = 10 * TIMER_MILLISECOND * 1000;
    }
    
    item = json_object_object_get(items, "log_level");
    if (item) {
        if (!json_object_is_type(item, json_type_string)) {
            fprintf(stderr, "log_level is not string\n");
            return -1;
        }
        const char* log_level = json_object_get_string(item);
        fprintf(stderr, "parse log level: %s\n", log_level);
        if      (strcasestr(log_level, "EMERG")) {
            ss_conf->log_level = RTE_LOG_EMERG;
        }
        else if (strcasestr(log_level, "ALERT")) {
            ss_conf->log_level = RTE_LOG_ALERT;
        }
        else if (strcasestr(log_level, "CRIT")) {
            ss_conf->log_level = RTE_LOG_CRIT;
        }
        else if (strcasestr(log_level, "ERR")) {
            ss_conf->log_level = RTE_LOG_ERR;
        }
        else if (strcasestr(log_level, "WARN")) {
            ss_conf->log_level = RTE_LOG_WARNING;
        }
        else if (strcasestr(log_level, "NOTICE")) {
            ss_conf->log_level = RTE_LOG_NOTICE;
        }
        else if (strcasestr(log_level, "INFO")) {
            ss_conf->log_level = RTE_LOG_INFO;
        }
        else if (strcasestr(log_level, "DEBUG")) {
            ss_conf->log_level = RTE_LOG_DEBUG;
        }
        else {
            fprintf(stderr, "could not parse log level: %s\n", log_level);
            ss_conf->log_level = RTE_LOG_WARNING;
        }
    }
    else {
        ss_conf->log_level = RTE_LOG_WARNING;
    }
    
    return 0;
}

ss_conf_t* ss_conf_file_parse() {
    int is_ok = 1;
    int rv;
    char* conf_buffer            = NULL;
    json_object* json_underlying = NULL;
    json_object* json_conf       = NULL;
    json_object* items           = NULL;
    json_object* item            = NULL;
    json_error_t json_error      = json_tokener_success;
    
    conf_buffer                  = ss_conf_file_read();
    if (conf_buffer == NULL) {
        fprintf(stderr, "conf file read error\n");
        is_ok = 0; goto error_out;
    }
    
    json_underlying = json_tokener_parse_verbose(conf_buffer, &json_error);
    if (json_underlying == NULL) {
        is_ok = 0;
        fprintf(stderr, "json parse error: %s\n", json_tokener_error_desc(json_error));
        is_ok = 0; goto error_out;
    }
    
    json_conf       = json_object_get(json_underlying);
    is_ok           = json_object_is_type(json_conf, json_type_object);
    if (!is_ok) {
        is_ok = 0;
        fprintf(stderr, "json configuration root is not object\n");
        is_ok = 0; goto error_out;
    }
    
    //const char* content = json_object_to_json_string_ext(json_conf, JSON_C_TO_STRING_PRETTY);
    //fprintf(stderr, "json configuration:\n%s\n", content);
    
    ss_conf = je_calloc(1, sizeof(ss_conf_t));
    if (ss_conf == NULL) {
        fprintf(stderr, "could not allocate sdn_sensor configuration\n");
        is_ok = 0; goto error_out;
    }
    TAILQ_INIT(&ss_conf->re_chain.re_list);
    TAILQ_INIT(&ss_conf->pcap_chain.pcap_list);
    TAILQ_INIT(&ss_conf->dns_chain.dns_list);
    TAILQ_INIT(&ss_conf->ioc_chain.ioc_list);
    // XXX: init more objects here
    
    items = json_object_object_get(json_conf, "network");
    if (items == NULL) {
        fprintf(stderr, "could not load network configuration\n");
        is_ok = 0; goto error_out;
    }
    if (!json_object_is_type(items, json_type_object)) {
        fprintf(stderr, "network configuration is not object\n");
        is_ok = 0; goto error_out;
    }
    
    rv = ss_conf_network_parse(items);
    if (rv) {
        fprintf(stderr, "could not parse network configuration\n");
        is_ok = 0; goto error_out;
    }
    
    items = json_object_object_get(json_conf, "dpdk");
    if (items == NULL) {
        fprintf(stderr, "could not load dpdk configuration\n");
        is_ok = 0; goto error_out;
    }
    if (!json_object_is_type(items, json_type_object)) {
        fprintf(stderr, "dpdk configuration is not object\n");
        is_ok = 0; goto error_out;
    }
    
    rv = ss_conf_dpdk_parse(items);
    if (rv) {
        fprintf(stderr, "could not parse network configuration\n");
        is_ok = 0; goto error_out;
    }
    
    items = json_object_object_get(json_conf, "re_chain");
    if (items) {
        is_ok = json_object_is_type(items, json_type_array);
        if (!is_ok) {
            fprintf(stderr, "re_chain is not an array\n");
            goto error_out;
        }
        int length = json_object_array_length(items);
        for (int i = 0; i < length; ++i) {
            item = json_object_array_get_idx(items, i);
            ss_re_entry_t* entry = ss_re_entry_create(item);
            /*
            if (entry == NULL) {
                fprintf(stderr, "could not create re_chain entry\n");
                if (entry) je_free(entry);
                is_ok = 0; goto error_out;
            }
            */
            if (entry == NULL) {
                fprintf(stderr, "could not create re_chain entry %d\n", i);
                ss_re_entry_destroy(entry);
                is_ok = 0; goto error_out;
            }
            ss_re_chain_add(entry);
        }
    }

    items = json_object_object_get(json_conf, "pcap_chain");
    if (items) {
        is_ok = json_object_is_type(items, json_type_array);
        if (!is_ok) {
            fprintf(stderr, "pcap_chain is not an array\n");
            goto error_out;
        }
        int length = json_object_array_length(items);
        for (int i = 0; i < length; ++i) {
            item = json_object_array_get_idx(items, i);
            ss_pcap_entry_t* entry = ss_pcap_entry_create(item);
            if (entry == NULL) {
                fprintf(stderr, "could not create pcap_chain entry %d\n", i);
                ss_pcap_entry_destroy(entry);
                is_ok = 0; goto error_out;
            }
            ss_pcap_chain_add(entry);
        }
    }
    
    items = json_object_object_get(json_conf, "dns_chain");
    if (items) {
        is_ok = json_object_is_type(items, json_type_array);
        if (!is_ok) {
            fprintf(stderr, "dns_chain is not an array\n");
            goto error_out;
        }
        int length = json_object_array_length(items);
        for (int i = 0; i < length; ++i) {
            item = json_object_array_get_idx(items, i);
            ss_dns_entry_t* entry = ss_dns_entry_create(item);
            if (entry == NULL) {
                fprintf(stderr, "could not create dns_chain entry %d\n", i);
                ss_dns_entry_destroy(entry);
                is_ok = 0; goto error_out;
            }
            ss_dns_chain_add(entry);
        }
    }

    items = json_object_object_get(json_conf, "cidr_table");
    if (items) {
        is_ok = json_object_is_type(items, json_type_array);
        if (!is_ok) {
            fprintf(stderr, "cidr_table is not an array\n");
            goto error_out;
        }
        int length = json_object_array_length(items);
        for (int i = 0; i < length; ++i) {
            item = json_object_array_get_idx(items, i);
            ss_cidr_entry_t* entry = ss_cidr_entry_create(item);
            /*
            if (entry == NULL) {
                fprintf(stderr, "could not create cidr_table entry %d\n", i);
                ss_cidr_entry_destroy(entry);
                is_ok = 0; goto error_out;
            }
            */
            ss_cidr_table_add(&ss_conf->cidr_table, entry);
        }
    }
    
    items = json_object_object_get(json_conf, "ioc_files");
    if (items) {
        is_ok = json_object_is_type(items, json_type_array);
        if (!is_ok) {
            fprintf(stderr, "ioc_files is not an array\n");
            goto error_out;
        }
        int length = json_object_array_length(items);
        if (length > SS_IOC_FILE_MAX) {
            fprintf(stderr, "ioc_file_count %d greater than %d, only parsing files below limit\n", length, SS_IOC_FILE_MAX);
            length = SS_IOC_FILE_MAX;
        }
        for (int i = 0; i < length; ++i) {
            item = json_object_array_get_idx(items, i);
            rv = ss_ioc_file_load(item);
            if (rv) {
                fprintf(stderr, "ioc_file index %d could not be loaded\n", i);
                is_ok = 0; goto error_out;
            }
        }
        
        ss_ioc_chain_dump(20);
        ss_ioc_chain_optimize();
        ss_ioc_tables_dump(5);
    }
    
    // XXX: do more stuff
    error_out:
    if (conf_buffer)       { je_free(conf_buffer);       conf_buffer = NULL; }
    if (json_conf)         { json_object_put(json_conf); json_conf   = NULL; }
    if (!is_ok && ss_conf) { ss_conf_destroy();          ss_conf     = NULL; }
    
    return ss_conf;
}
