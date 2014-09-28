#ifndef __METADATA_H__
#define __METADATA_H__

#include <json-c/json.h>
#include <json-c/json_object_private.h>

#include "common.h"
#include "ioc.h"
#include "nn_queue.h"

/* BEGIN PROTOTYPES */

int ss_metadata_prepare_eth(const char* source, nn_queue_t* nn_queue, json_object* jobject, ss_frame_t* fbuf);
int ss_metadata_prepare_ip(const char* source, nn_queue_t* nn_queue, json_object* jobject, ss_frame_t* fbuf);
int ss_metadata_prepare_ioc(const char* source, nn_queue_t* nn_queue, ss_ioc_entry_t* iptr, json_object* json);
uint8_t* ss_metadata_prepare_frame(const char* source, nn_queue_t* nn_queue, ss_frame_t* fbuf, ss_ioc_entry_t* iptr);
uint8_t* ss_metadata_prepare_syslog(const char* source, nn_queue_t* nn_queue, ss_frame_t* fbuf, ss_ioc_entry_t* iptr, uint8_t* message, int length);

/* END PROTOTYPES */

#endif /* __METADATA_H__ */