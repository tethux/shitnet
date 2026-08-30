#include <cstddef>
#include <cstdint>
#include <shitnet/macros.h>
#include <shitnet/shitnet.h>

#include <span>

import shitnet.address;
import shitnet.stack;

struct shitnet {
    Stack stack;
};

cfn shitnet_create(const shitnet_config *config) -> shitnet_t * {
    try {
        if (config == nullptr)
            return nullptr;

        MacAddress mac{};
        for (std::size_t i = 0; i < mac.bytes.size(); ++i)
            mac.bytes[i] = std::byte{config->mac[i]};

        IPv4Address ip{};
        for (std::size_t i = 0; i < ip.bytes.size(); ++i)
            ip.bytes[i] = std::byte{config->ip[i]};

        return new shitnet{
            .stack =
                {
                    .mac = mac,
                    .ip = ip,
                    .arp_table = {},
                    .tx = {},
                    .events = {},
                    .current_event = {},
                },
        };
    } catch (...) {
        return nullptr;
    }
}

cfn shitnet_destroy(shitnet_t *instance) -> void {
    try {
        delete instance;
    } catch (...) {
    }
}

cfn shitnet_tx_size(const shitnet_t *instance) -> size_t {
    try {
        if (instance == nullptr)
            return 0;

        return stack_tx_size(instance->stack);
    } catch (...) {
        return 0;
    }
}

cfn shitnet_arp_lookup(const shitnet_t *instance, const uint8_t ip[4],
                       uint8_t mac[6]) -> int {
    try {
        if (instance == nullptr || ip == nullptr || mac == nullptr)
            return SHITNET_ERR_INVALID_ARGUMENT;

        IPv4Address address{};
        for (std::size_t i = 0; i < address.bytes.size(); ++i)
            address.bytes[i] = std::byte{ip[i]};

        MacAddress result{};
        if (!stack_arp_lookup(instance->stack, address, result))
            return SHITNET_LOOKUP_MISS;

        for (std::size_t i = 0; i < result.bytes.size(); ++i)
            mac[i] = std::to_integer<std::uint8_t>(result.bytes[i]);

        return SHITNET_LOOKUP_FOUND;
    } catch (...) {
        return SHITNET_ERR_INTERNAL;
    }
}

cfn shitnet_arp_count(const shitnet_t *instance) -> size_t {
    try {
        if (instance == nullptr)
            return 0;

        return stack_arp_count(instance->stack);
    } catch (...) {
        return 0;
    }
}

cfn shitnet_arp_entry(const shitnet_t *instance, size_t index, uint8_t ip[4],
                      uint8_t mac[6]) -> int {
    try {
        if (instance == nullptr || ip == nullptr || mac == nullptr)
            return SHITNET_ERR_INVALID_ARGUMENT;

        IPv4Address address{};
        MacAddress hardware{};
        if (!stack_arp_entry(instance->stack, index, address, hardware))
            return SHITNET_LOOKUP_MISS;

        for (std::size_t i = 0; i < address.bytes.size(); ++i)
            ip[i] = std::to_integer<std::uint8_t>(address.bytes[i]);
        for (std::size_t i = 0; i < hardware.bytes.size(); ++i)
            mac[i] = std::to_integer<std::uint8_t>(hardware.bytes[i]);
        return SHITNET_LOOKUP_FOUND;
    } catch (...) {
        return SHITNET_ERR_INTERNAL;
    }
}

cfn shitnet_arp_request(shitnet_t *instance, const uint8_t target_ip[4])
    -> int {
    try {
        if (instance == nullptr || target_ip == nullptr)
            return SHITNET_ERR_INVALID_ARGUMENT;

        IPv4Address ip{};
        for (std::size_t i = 0; i < ip.bytes.size(); ++i)
            ip.bytes[i] = std::byte{target_ip[i]};

        stack_arp_request(instance->stack, ip);
        return SHITNET_QUEUE_QUEUED;
    } catch (...) {
        return SHITNET_ERR_INTERNAL;
    }
}

cfn shitnet_poll_tx(shitnet_t *instance, uint8_t *buffer, size_t buffer_size,
                    size_t *written) -> int {
    try {
        if (instance == nullptr || buffer == nullptr || written == nullptr)
            return SHITNET_ERR_INVALID_ARGUMENT;

        let bytes = std::span{
            reinterpret_cast<std::byte *>(buffer),
            buffer_size,
        };
        const let result = stack_poll_tx(instance->stack, bytes, *written);
        if (result == -2)
            return SHITNET_ERR_BUFFER_TOO_SMALL;

        return result;
    } catch (...) {
        return SHITNET_ERR_INTERNAL;
    }
}

cfn shitnet_receive(shitnet_t *instance, const uint8_t *data, size_t len)
    -> int {
    try {
        if (instance == nullptr || data == nullptr)
            return SHITNET_ERR_INVALID_ARGUMENT;

        const let bytes = std::span{
            reinterpret_cast<const std::byte *>(data),
            len,
        };
        const let result = stack_receive(instance->stack, bytes);
        if (result < 0)
            return SHITNET_ERR_INVALID_PACKET;

        return SHITNET_OK;
    } catch (...) {
        return SHITNET_ERR_INTERNAL;
    }
}

cfn shitnet_icmp_echo_request(shitnet_t *instance, const uint8_t target_ip[4],
                              uint16_t identifier, uint16_t sequence,
                              const uint8_t *payload, size_t payload_len)
    -> int {
    try {
        if (instance == nullptr || target_ip == nullptr ||
            (payload == nullptr && payload_len != 0) || payload_len > 65507)
            return SHITNET_ERR_INVALID_ARGUMENT;

        IPv4Address ip{};
        for (std::size_t i = 0; i < ip.bytes.size(); ++i)
            ip.bytes[i] = std::byte{target_ip[i]};

        const let bytes = std::span{
            reinterpret_cast<const std::byte *>(payload), payload_len};
        return stack_icmp_echo_request(instance->stack, ip, identifier,
                                       sequence, bytes)
                   ? SHITNET_QUEUE_QUEUED
                   : SHITNET_QUEUE_UNRESOLVED;
    } catch (...) {
        return SHITNET_ERR_INTERNAL;
    }
}

cfn shitnet_poll_event(shitnet_t *instance, shitnet_event *event) -> int {
    try {
        if (instance == nullptr || event == nullptr)
            return SHITNET_ERR_INVALID_ARGUMENT;

        const let *value = stack_poll_event(instance->stack);
        if (value == nullptr) {
            event->type = SHITNET_EVENT_NONE;
            return 0;
        }

        switch (value->type) {
        case StackEventType::arpLearned:
            event->type = SHITNET_EVENT_ARP_LEARNED;
            for (std::size_t i = 0; i < 4; ++i)
                event->data.arp.ip[i] =
                    std::to_integer<uint8_t>(value->source_ip.bytes[i]);
            for (std::size_t i = 0; i < 6; ++i)
                event->data.arp.mac[i] =
                    std::to_integer<uint8_t>(value->mac.bytes[i]);
            break;
        case StackEventType::icmpEchoRequest:
        case StackEventType::icmpEchoReply:
            event->type = value->type == StackEventType::icmpEchoRequest
                              ? SHITNET_EVENT_ICMP_ECHO_REQUEST
                              : SHITNET_EVENT_ICMP_ECHO_REPLY;
            for (std::size_t i = 0; i < 4; ++i)
                event->data.icmp_echo.source_ip[i] =
                    std::to_integer<uint8_t>(value->source_ip.bytes[i]);
            event->data.icmp_echo.identifier = value->identifier;
            event->data.icmp_echo.sequence = value->sequence;
            event->data.icmp_echo.payload =
                reinterpret_cast<const uint8_t *>(value->payload.data());
            event->data.icmp_echo.payload_len = value->payload.size();
            break;
        case StackEventType::none:
            event->type = SHITNET_EVENT_NONE;
            break;
        }
        return 1;
    } catch (...) {
        return SHITNET_ERR_INTERNAL;
    }
}
