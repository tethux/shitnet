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
    packet[14] = 0x00;
    packet[15] = 0x01;
    packet[16] = 0x08;
    packet[17] = 0x00;
    packet[18] = 6;
    packet[19] = 4;
    packet[20] = 0x00;
    packet[21] = 0x01;
    packet[38] = 10;
    packet[39] = 0;
    packet[40] = 0;
    packet[41] = 2;

    assert(shitnet_receive(instance, packet.data(), packet.size()) == 1);
    shitnet_destroy(instance);
    return 0;
}
