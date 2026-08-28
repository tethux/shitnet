#include <array>
#include <cassert>
#include <cstdint>

import shitnet;

int main() {
    shitnet *instance = shitnet_create();
    assert(instance != nullptr);

    std::array<std::uint8_t, 42> packet{};
    packet[12] = 0x08;
    packet[13] = 0x06;
    packet[20] = 0x00;
    packet[21] = 0x01;

    assert(shitnet_receive(instance, packet.data(), packet.size()) == 1);
    shitnet_destroy(instance);
    return 0;
}
