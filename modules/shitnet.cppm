module;

#include <shitnet/macros.h>

#include <shitnet/shitnet.h>

export module shitnet;

export using ::shitnet_t;
export using ::shitnet_arp_event;
export using ::shitnet_config;
export using ::shitnet_event;
export using ::shitnet_event_type;
export using ::shitnet_icmp_echo_event;
export using ::shitnet_lookup_result;
export using ::shitnet_queue_result;
export using ::shitnet_result;
export using ::shitnet_create;
export using ::shitnet_destroy;
export using ::shitnet_arp_count;
export using ::shitnet_arp_entry;
export using ::shitnet_arp_lookup;
export using ::shitnet_arp_request;
export using ::shitnet_icmp_echo_request;
export using ::shitnet_poll_event;
export using ::shitnet_poll_tx;
export using ::shitnet_receive;
export using ::shitnet_tx_size;
