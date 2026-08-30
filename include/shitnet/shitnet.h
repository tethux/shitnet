#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct shitnet shitnet_t;

typedef struct shitnet_config {
    uint8_t mac[6];
    uint8_t ip[4];
} shitnet_config;

typedef enum shitnet_result {
    SHITNET_OK = 0,
    SHITNET_ERR_INVALID_ARGUMENT = -1,
    SHITNET_ERR_INVALID_PACKET = -2,
    SHITNET_ERR_BUFFER_TOO_SMALL = -3,
    SHITNET_ERR_INTERNAL = -4,
} shitnet_result;

typedef enum shitnet_lookup_result {
    SHITNET_LOOKUP_MISS = 0,
    SHITNET_LOOKUP_FOUND = 1,
} shitnet_lookup_result;

typedef enum shitnet_queue_result {
    SHITNET_QUEUE_UNRESOLVED = 0,
    SHITNET_QUEUE_QUEUED = 1,
} shitnet_queue_result;

typedef enum shitnet_event_type {
    SHITNET_EVENT_NONE = 0,

    SHITNET_EVENT_ARP_LEARNED,
    SHITNET_EVENT_ICMP_ECHO_REQUEST,
    SHITNET_EVENT_ICMP_ECHO_REPLY,
} shitnet_event_type;

typedef struct shitnet_arp_event {
    uint8_t ip[4];
    uint8_t mac[6];
} shitnet_arp_event;

typedef struct shitnet_icmp_echo_event {
    uint8_t source_ip[4];
    uint16_t identifier;
    uint16_t sequence;

    /*
     * Payload belongs to shitnet.
     * Valid until the next receive/event operation unless documented
     * otherwise.
     */
    const uint8_t *payload;
    size_t payload_len;
} shitnet_icmp_echo_event;

typedef struct shitnet_event {
    shitnet_event_type type;

    union {
        shitnet_arp_event arp;
        shitnet_icmp_echo_event icmp_echo;
    } data;
} shitnet_event;

/*
 * Lifecycle
 */

shitnet_t *shitnet_create(const shitnet_config *config);

void shitnet_destroy(shitnet_t *instance);

/*
 * Dataplane RX/TX
 */

int shitnet_receive(shitnet_t *instance, const uint8_t *data, size_t len);

size_t shitnet_tx_size(const shitnet_t *instance);

int shitnet_poll_tx(shitnet_t *instance, uint8_t *buffer, size_t buffer_size,
                    size_t *written);

/*
 * ARP
 */

int shitnet_arp_lookup(const shitnet_t *instance, const uint8_t ip[4],
                       uint8_t mac[6]);

size_t shitnet_arp_count(const shitnet_t *instance);

int shitnet_arp_entry(const shitnet_t *instance, size_t index, uint8_t ip[4],
                      uint8_t mac[6]);

int shitnet_arp_request(shitnet_t *instance, const uint8_t target_ip[4]);

/*
 * ICMP
 */

int shitnet_icmp_echo_request(shitnet_t *instance, const uint8_t target_ip[4],
                              uint16_t identifier, uint16_t sequence,
                              const uint8_t *payload, size_t payload_len);

/*
 * Events
 */

int shitnet_poll_event(shitnet_t *instance, shitnet_event *event);

#ifdef __cplusplus
}
#endif
