#include <array>
#include <cassert>
#include <cstdint>
#include <shitnet/macros.h>

import shitnet;

int main() {
    const shitnet_config config{
        .mac = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02},
        .ip = {10, 0, 0, 2},
    };
    shitnet *instance = shitnet_create(&config);
    assert(instance != nullptr);

    const std::array<std::uint8_t, 4> target{10, 0, 0, 1};
    assert(shitnet_arp_request(instance, target.data()) ==
           SHITNET_QUEUE_QUEUED);

    std::array<std::uint8_t, 42> request{};
    std::size_t request_size = 0;
    assert(shitnet_poll_tx(instance, request.data(), request.size(),
                           &request_size) == 1);
    assert(request_size == request.size());
    for (std::size_t i = 0; i < 6; ++i)
        assert(request[i] == 0xff);
    assert(request[20] == 0x00);
    assert(request[21] == 0x01);
    assert(request[38] == 10);
    assert(request[39] == 0);
    assert(request[40] == 0);
    assert(request[41] == 1);

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

    assert(shitnet_receive(instance, packet.data(), packet.size()) ==
           SHITNET_OK);
    shitnet_destroy(instance);
    return 0;
}
