module;

#include <array>
#include <cstddef>

export module shitnet.address;

export struct MacAddress {
    std::array<std::byte, 6> bytes;
};

export struct IPv4Address {
    std::array<std::byte, 4> bytes;
};
