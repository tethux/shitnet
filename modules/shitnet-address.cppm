module;

#include <shitnet/macros.h>

#include <array>
#include <cstddef>

export module shitnet.address;

export struct MacAddress {
    std::array<std::byte, 6> bytes;

    bool operator==(const MacAddress &) const = default;
};

export struct IPv4Address {
    std::array<std::byte, 4> bytes;
    bool operator==(const IPv4Address &) const = default;
};
