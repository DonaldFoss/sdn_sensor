#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>

#include "metadata.h"
#include "common.h"
#include "ioc.h"
#include "json.h"
#include "nn_queue.h"

int ss_metadata_prepare_eth(const char* source, nn_queue_t* nn_queue, json_object* jobject, ss_frame_t* fbuf) {
    char tmp[1024];
    json_object* jsource   = NULL;
    json_object* port_id   = NULL;
    json_object* direction = NULL;
    json_object* self      = NULL;
    json_object* length    = NULL;
    json_object* eth_type  = NULL;
    json_object* smac      = NULL;
    json_object* dmac      = NULL;
    
    jsource     = json_object_new_string(source);
    if (jsource == NULL) goto error_out;
    port_id     = json_object_new_int(fbuf->data.port_id);
    if (port_id == NULL) goto error_out;
    direction   = json_object_new_int(fbuf->data.direction);
    if (direction == NULL) goto error_out;
    self        = json_object_new_int(fbuf->data.self);
    if (self == NULL) goto error_out;
    length      = json_object_new_int(fbuf->data.length);
    if (length == NULL) goto error_out;
    eth_type    = json_object_new_int(fbuf->data.eth_type);
    if (eth_type == NULL) goto error_out;

    snprintf(tmp, sizeof(tmp), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
        fbuf->data.smac[0], fbuf->data.smac[1], fbuf->data.smac[2],
        fbuf->data.smac[3], fbuf->data.smac[4], fbuf->data.smac[5]);
    smac        = json_object_new_string(tmp);
    if (smac == NULL) goto error_out;
    
    snprintf(tmp, sizeof(tmp), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
        fbuf->data.dmac[0], fbuf->data.dmac[1], fbuf->data.dmac[2],
        fbuf->data.dmac[3], fbuf->data.dmac[4], fbuf->data.dmac[5]);
    dmac        = json_object_new_string(tmp);
    if (dmac == NULL) goto error_out;
        
    json_object_object_add(jobject, "source",    jsource);
    json_object_object_add(jobject, "port_id",   port_id);
    json_object_object_add(jobject, "direction", direction);
    json_object_object_add(jobject, "self",      self);
    json_object_object_add(jobject, "length",    length);
    json_object_object_add(jobject, "eth_type",  eth_type);
    json_object_object_add(jobject, "smac",      smac);
    json_object_object_add(jobject, "dmac",      dmac);
    
    return 0;
    
    error_out:
    fprintf(stderr, "could not serialize ethernet metadata\n");
    if (jsource)   json_object_put(jsource);
    if (port_id)   json_object_put(port_id);
    if (direction) json_object_put(direction);
    if (self)      json_object_put(self);
    if (length)    json_object_put(length);
    if (smac)      json_object_put(smac);
    if (dmac)      json_object_put(dmac);
    if (eth_type)  json_object_put(eth_type);
    
    return -1;
}

int ss_metadata_prepare_ip(const char* source, nn_queue_t* nn_queue, json_object* jobject, ss_frame_t* fbuf) {
    char tmp[1024];
    json_object* sip         = NULL;
    json_object* dip         = NULL;
    json_object* ip_protocol = NULL;
    json_object* ttl         = NULL;
    json_object* l4_length   = NULL;
    json_object* icmp_type   = NULL;
    json_object* icmp_code   = NULL;
    json_object* sport       = NULL;
    json_object* dport       = NULL;
    json_object* dns_name    = NULL;
    
    if      (fbuf->data.eth_type == ETHER_TYPE_IPV4) {
        snprintf(tmp, sizeof(tmp), "%hhu.%hhu.%hhu.%hhu",
            fbuf->data.sip[0], fbuf->data.sip[1], fbuf->data.sip[2], fbuf->data.sip[3]);
        sip = json_object_new_string(tmp);
        snprintf(tmp, sizeof(tmp), "%hhu.%hhu.%hhu.%hhu",
            fbuf->data.dip[0], fbuf->data.dip[1], fbuf->data.dip[2], fbuf->data.dip[3]);
        dip = json_object_new_string(tmp);
    }
    else if (fbuf->data.eth_type == ETHER_TYPE_IPV6) {
        snprintf(tmp, sizeof(tmp), "%hx:%hx:%hx:%hx:%hx:%hx:%hx:%hx",
            *(uint16_t*) &fbuf->data.sip[0],  *(uint16_t*) &fbuf->data.sip[2],
            *(uint16_t*) &fbuf->data.sip[4],  *(uint16_t*) &fbuf->data.sip[6],
            *(uint16_t*) &fbuf->data.sip[8],  *(uint16_t*) &fbuf->data.sip[10],
            *(uint16_t*) &fbuf->data.sip[12], *(uint16_t*) &fbuf->data.sip[14]);
        sip = json_object_new_string(tmp);
        snprintf(tmp, sizeof(tmp), "%hx:%hx:%hx:%hx:%hx:%hx:%hx:%hx",
            *(uint16_t*) &fbuf->data.sip[0],  *(uint16_t*) &fbuf->data.sip[2],
            *(uint16_t*) &fbuf->data.sip[4],  *(uint16_t*) &fbuf->data.sip[6],
            *(uint16_t*) &fbuf->data.sip[8],  *(uint16_t*) &fbuf->data.sip[10],
            *(uint16_t*) &fbuf->data.sip[12], *(uint16_t*) &fbuf->data.sip[14]);
        dip = json_object_new_string(tmp);
    }
    else {
        fprintf(stderr, "could not extract IP addresses\n");
    }
    
    ip_protocol = json_object_new_int(fbuf->data.ip_protocol);
    if (ip_protocol == NULL) goto error_out;
    ttl         = json_object_new_int(fbuf->data.ttl);
    if (ttl == NULL) goto error_out;
    l4_length   = json_object_new_int(fbuf->data.l4_length);
    if (l4_length == NULL) goto error_out;
    icmp_type   = json_object_new_int(fbuf->data.icmp_type);
    if (icmp_type == NULL) goto error_out;
    icmp_code   = json_object_new_int(fbuf->data.icmp_code);
    if (icmp_code == NULL) goto error_out;
    sport       = json_object_new_int(fbuf->data.sport);
    if (sport == NULL) goto error_out;
    dport       = json_object_new_int(fbuf->data.dport);
    if (dport == NULL) goto error_out;
    dns_name    = json_object_new_string((char*)fbuf->data.dns_name);
    if (dns_name == NULL) goto error_out;    

    json_object_object_add(jobject, "sip",         sip);
    json_object_object_add(jobject, "dip",         dip);
    json_object_object_add(jobject, "ip_protocol", ip_protocol);
    json_object_object_add(jobject, "ttl",         ttl);
    json_object_object_add(jobject, "l4_length",   l4_length);
    json_object_object_add(jobject, "icmp_type",   icmp_type);
    json_object_object_add(jobject, "icmp_code",   icmp_code);
    json_object_object_add(jobject, "sport",       sport);
    json_object_object_add(jobject, "dport",       dport);
    json_object_object_add(jobject, "dns_name",    dns_name);
    // XXX: add support for dns_answers field, dns query type
    
    return 0;
    
    error_out:
    fprintf(stderr, "could not serialize ethernet metadata\n");
    if (sip)         json_object_put(sip);
    if (dip)         json_object_put(dip);
    if (ip_protocol) json_object_put(ip_protocol);
    if (ttl)         json_object_put(ttl);
    if (l4_length)   json_object_put(l4_length);
    if (icmp_type)   json_object_put(icmp_type);
    if (icmp_code)   json_object_put(icmp_code);
    if (sport)       json_object_put(sport);
    if (dport)       json_object_put(dport);
    if (dns_name)    json_object_put(dns_name);
    
    return -1;
}

int ss_metadata_prepare_ioc(const char* source, nn_queue_t* nn_queue, ss_ioc_entry_t* iptr, json_object* json) {
    char ip_str[SS_ADDR_STR_MAX];
    const char* result;
    
    json_object* file_id     = NULL;
    json_object* ioc_id      = NULL;
    json_object* type        = NULL;
    json_object* threat_type = NULL;
    json_object* ip          = NULL;
    json_object* value       = NULL;
    json_object* dns         = NULL;
    
    if (nn_queue->format != NN_FORMAT_METADATA) {
        fprintf(stderr, "format %d not supported yet\n", nn_queue->format);
        goto error_out;
    }
    
    file_id     = json_object_new_int(iptr->file_id);
    if (file_id == NULL) goto error_out;
    ioc_id      = json_object_new_int(iptr->id);
    if (ioc_id == NULL) goto error_out;
    type        = json_object_new_string(ss_ioc_type_dump(iptr->type));
    if (type == NULL) goto error_out;
    threat_type = json_object_new_string(iptr->threat_type);
    if (threat_type == NULL) goto error_out;
    result = ss_inet_ntop(&iptr->ip, ip_str, sizeof(ip_str));
    if (result != NULL) {
        ip = json_object_new_string(ip_str);
    }
    if (ip == NULL) goto error_out;
    value       = json_object_new_string(iptr->value);
    if (value == NULL) goto error_out;
    dns         = json_object_new_string(iptr->dns);
    if (dns == NULL) goto error_out;
    
    json_object_object_add(json, "file_id",     file_id);
    json_object_object_add(json, "ioc_id",      ioc_id);
    json_object_object_add(json, "type",        type);
    json_object_object_add(json, "threat_type", threat_type);
    json_object_object_add(json, "ip",          ip);
    json_object_object_add(json, "value",       value);
    json_object_object_add(json, "dns",         dns);
    
    return 0;
    
    error_out:
    fprintf(stderr, "could not serialize ioc id: %lu\n", iptr->id);
    if (file_id)     json_object_put(file_id);
    if (ioc_id)      json_object_put(ioc_id);
    if (type)        json_object_put(type);
    if (threat_type) json_object_put(threat_type);
    if (ip)          json_object_put(ip);
    if (value)       json_object_put(value);
    if (dns)         json_object_put(dns);
    
    return -1;
}

uint8_t* ss_metadata_prepare_frame(const char* source, nn_queue_t* nn_queue, ss_frame_t* fbuf, ss_ioc_entry_t* iptr) {
    int          irv;
    uint8_t*     rv;
    json_object* jobject = NULL;
    
    if (nn_queue->format != NN_FORMAT_METADATA) {
        fprintf(stderr, "format %d not supported yet\n", nn_queue->format);
        goto error_out;
    }
    
    jobject = json_object_new_object();
    if (jobject == NULL) {
        fprintf(stderr, "could not allocate json object\n");
        goto error_out;
    }
    
    irv = ss_metadata_prepare_eth(source, nn_queue, jobject, fbuf);
    if (irv) goto error_out;
    
    irv = ss_metadata_prepare_ip(source, nn_queue, jobject, fbuf);
    if (irv) goto error_out;
    
    if (iptr) {
        irv = ss_metadata_prepare_ioc(source, nn_queue, iptr, jobject);
        if (irv) goto error_out;
    }
    
    rv = (uint8_t*) json_object_to_json_string_ext(jobject, JSON_C_TO_STRING_SPACED);
    json_object_put(jobject);
    return rv;
    
    error_out:
    fprintf(stderr, "could not serialize packet metadata\n");
    return NULL;
}

uint8_t* ss_metadata_prepare_syslog(const char* source, nn_queue_t* nn_queue, ss_frame_t* fbuf, ss_ioc_entry_t* iptr) {
    int          irv;
    uint8_t*     rv;
    json_object* jmessage = NULL;
    json_object* jobject = NULL;
    
    if (nn_queue->format != NN_FORMAT_METADATA) {
        fprintf(stderr, "format %d not supported yet\n", nn_queue->format);
        goto error_out;
    }
    
    jobject = json_object_new_object();
    if (jobject == NULL) {
        fprintf(stderr, "could not allocate json object\n");
        goto error_out;
    }
    
    irv = ss_metadata_prepare_ip(source, nn_queue, jobject, fbuf);
    if (irv) goto error_out;
    
    if (iptr) {
        irv = ss_metadata_prepare_ioc(source, nn_queue, iptr, jobject);
        if (irv) goto error_out;
    }
    
    // XXX: for now assume the message is C char*
    jmessage = json_object_new_string((char*) fbuf->l4_offset);
    if (jmessage == NULL) goto error_out;
    json_object_object_add(jobject, "message", jmessage);
    
    rv = (uint8_t*) json_object_to_json_string_ext(jobject, JSON_C_TO_STRING_SPACED);
    json_object_put(jobject);
    return rv;
    
    error_out:
    fprintf(stderr, "could not create syslog metadata\n");
    if (jmessage) json_object_put(jmessage);
    if (jobject)  json_object_put(jobject);
    
    return NULL;
}
