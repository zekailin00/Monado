#pragma once

#include <pthread.h>
#include <stdbool.h>
#include "offload_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif


void *socket_thread(void* arg);

void tx_enqueue(message_packet_t* packet);
bool rx_dequeue(message_packet_t* packet);

#ifdef __cplusplus
}
#endif
