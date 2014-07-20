#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <bsd/string.h>
#include <json-c/json.h>

#include "common.h"
#include "sensor_configuration.h"

#define PROGRAM_PATH "/proc/self/exe"
#define CONF_PATH "/../conf/sdn_sensor.json"

char* ss_conf_path_get() {
    size_t path_size = PATH_MAX;
    char* program_path = calloc(1, path_size);
    if (!program_path) return NULL;
    
    ssize_t rv = readlink(PROGRAM_PATH, program_path, path_size);
    if (rv < 0) return NULL;
    
    char* directory = dirname(program_path);
    char* program_directory = calloc(1, path_size);
    strlcpy(program_directory, directory, path_size);
    strlcat(program_directory, CONF_PATH, path_size);
    
    if (program_directory == NULL) return NULL;
    
    return program_directory;
}

char* ss_conf_file_read() {
    int is_ok = 1;
    int rv;
    size_t srv;
    
    char* conf_path = ss_conf_path_get();
    
    FILE* conf_file = fopen(conf_path, "rb");
    rv = fseek(conf_file, 0L, SEEK_END);
    if (rv == -1) {
        is_ok = 0;
        fprintf(stderr, "error: could not seek to end of configuration file\n");
        goto error_out;
    }
    size_t size = ftell(conf_file) + 1;
    if (size == (unsigned long) -1) {
        is_ok = 0;
        fprintf(stderr, "error: could not get size of configuration file\n");
        goto error_out;
    }
    rewind(conf_file);
    
    char* conf_content = calloc(1, size);
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
    conf_content[size - 1] = '\0';
    
    return conf_content;
    
    error_out:
    if (conf_path)    { free(conf_path);    conf_path    = NULL; }
    if (conf_content) { free(conf_content); conf_content = NULL; }
    if (conf_file)    { fclose(conf_file);  conf_file    = NULL; }
    return NULL;
}

struct cidr* ss_parse_cidr(char* cidr) {
    return NULL;
}

struct sockaddr* ss_parse_ip(char* ip) {
    int rv;
    struct sockaddr* addr = calloc(1, sizeof(struct sockaddr));
    
    rv = inet_pton(AF_INET, ip, addr);
    if (rv == 1) return addr;
    
    rv = inet_pton(AF_INET6, ip, addr);
    
    if (addr) free(addr);
    return NULL;
}

ss_conf_t* ss_conf_file_parse() {
    int is_ok = 1;
    ss_conf_t* ss_conf           = NULL;
    char* conf_buffer            = NULL;
    json_object* json_underlying = NULL;
    json_object* json_conf       = NULL;
    json_object* items           = NULL;
    json_object* item            = NULL;
    json_error_t json_error      = json_tokener_success;
    
    conf_buffer     = ss_conf_file_read();
    json_underlying = json_tokener_parse_verbose(conf_buffer, &json_error);
    if (json_underlying == NULL) {
        is_ok = 0;
        fprintf(stderr, "json parse error: %s\n", json_tokener_error_desc(json_error));
        goto error_out;
    }
    json_conf       = json_object_get(json_underlying);
    is_ok           = json_object_is_type(json_conf, json_type_object);
    if (!is_ok) {
        fprintf(stderr, "json root is not object\n");
        goto error_out;
    }
    
    const char* content = json_object_to_json_string_ext(json_conf, JSON_C_TO_STRING_PRETTY);
    printf("json configuration:\n%s", content);
    
    ss_conf = calloc(1, sizeof(ss_conf_t));
    if (ss_conf == NULL) {
        is_ok = 0;
        fprintf(stderr, "could not allocate sdn_sensor configuration\n");
        goto error_out;    
    }
    
    items = json_object_object_get(json_conf, "options");
    if (items == NULL) {
        is_ok = 0;
        fprintf(stderr, "could not get options\n");
        goto error_out;
    }
    item = json_object_object_get(items, "promiscuous_mode");
    if (item) {
        ss_conf->promiscuous_mode = json_object_get_boolean(item);
    }
    
    items = json_object_object_get(json_conf, "re_chain");
    if (items) {
        is_ok = json_object_is_type(json_conf, json_type_array);
        if (!is_ok) {
            fprintf(stderr, "re_chain is not an array\n");
            goto error_out;
        }
        int length = json_object_array_length(items);
        for (int i = 0; i < length; ++i) {
            item = json_object_array_get_idx(items, i);
            ss_re_entry_t* entry = calloc(1, sizeof(ss_re_entry_t));
            entry = ss_re_entry_create(item);
            ss_re_chain_add(&ss_conf->re_chain, entry);
        }
    }

    items = json_object_object_get(json_conf, "pcap_chain");
    if (items) {
        is_ok = json_object_is_type(json_conf, json_type_array);
        if (!is_ok) {
            fprintf(stderr, "pcap_chain is not an array\n");
            goto error_out;
        }
        int length = json_object_array_length(items);
        for (int i = 0; i < length; ++i) {
            item = json_object_array_get_idx(items, i);
            ss_pcap_entry_t* entry = calloc(1, sizeof(ss_pcap_entry_t));
            entry = ss_pcap_entry_create(item);
            ss_pcap_chain_add(&ss_conf->pcap_chain, entry);
        }
    }

    items = json_object_object_get(json_conf, "cidr_table");
    if (items) {
        is_ok = json_object_is_type(json_conf, json_type_object);
        if (!is_ok) {
            fprintf(stderr, "cidr_table is not an object\n");
            goto error_out;
        }
        json_object_object_foreach(items, cidr, cidr_conf) {
            is_ok = json_object_is_type(cidr_conf, json_type_object);
            if (!is_ok) {
                fprintf(stderr, "cidr_table entry is not an object\n");
                goto error_out;
            }
            ss_cidr_entry_t* entry = calloc(1, sizeof(ss_cidr_entry_t));
            entry = ss_cidr_entry_create(cidr_conf);
            ss_cidr_table_add(&ss_conf->cidr_table, entry);
        }
    }
    
    // XXX: do more stuff
    
    error_out:
    if (conf_buffer)       { free(conf_buffer);          conf_buffer = NULL; }
    if (json_conf)         { json_object_put(json_conf); json_conf   = NULL; }
    if (!is_ok && ss_conf) { free(ss_conf);              ss_conf     = NULL; }
    
    return ss_conf;
}
