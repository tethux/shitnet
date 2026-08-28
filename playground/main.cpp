#include <array>
#include <cstdint>
#include <cstdio>

import shitnet;

int main() {
    std::array<std::uint8_t, 42> packet{};
    packet[12] = 0x08;
    packet[13] = 0x06;
    packet[20] = 0x00;
    packet[21] = 0x01;

    shitnet *net = shitnet_create();
    if (net == nullptr) {
        return 1;
    }

    const int result = shitnet_receive(net, packet.data(), packet.size());
    std::printf("result = %d\n", result);
    shitnet_destroy(net);
    return 0;
}
