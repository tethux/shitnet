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

cfn shitnet_create(const shitnet_config *config) -> shitnet * {
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
                },
        };
    } catch (...) {
        return nullptr;
    }
}

cfn shitnet_destroy(shitnet *instance) -> void {
    try {
        delete instance;
    } catch (...) {
    }
}

cfn shitnet_tx_size(const shitnet *instance) -> size_t {
    try {
        if (instance == nullptr)
            return 0;

        return stack_tx_size(instance->stack);
    } catch (...) {
        return 0;
    }
}

cfn shitnet_arp_lookup(const shitnet *instance, const uint8_t ip[4],
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

cfn shitnet_arp_request(shitnet *instance, const uint8_t target_ip[4]) -> int {
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

cfn shitnet_poll_tx(shitnet *instance, uint8_t *buffer, size_t buffer_size,
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

cfn shitnet_receive(shitnet *instance, const uint8_t *data, size_t len) -> int {
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
