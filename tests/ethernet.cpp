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
    shitnet_t *instance = shitnet_create(&config);
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
    packet[21] = 0x02;
    const std::array<std::uint8_t, 6> peer_mac{0x02, 0x00, 0x00,
                                               0x00, 0x00, 0x01};
    for (std::size_t i = 0; i < peer_mac.size(); ++i)
        packet[22 + i] = peer_mac[i];
    for (std::size_t i = 0; i < target.size(); ++i)
        packet[28 + i] = target[i];

    assert(shitnet_receive(instance, packet.data(), packet.size()) ==
           SHITNET_OK);

    shitnet_event event{};
    assert(shitnet_poll_event(instance, &event) == 1);
    assert(event.type == SHITNET_EVENT_ARP_LEARNED);
    for (std::size_t i = 0; i < target.size(); ++i)
        assert(event.data.arp.ip[i] == target[i]);
    for (std::size_t i = 0; i < peer_mac.size(); ++i)
        assert(event.data.arp.mac[i] == peer_mac[i]);

    const std::array<std::uint8_t, 4> payload{'p', 'i', 'n', 'g'};
    assert(shitnet_icmp_echo_request(instance, target.data(), 0x1234, 7,
                                     payload.data(),
                                     payload.size()) == SHITNET_QUEUE_QUEUED);

    std::array<std::uint8_t, 46> echo{};
    std::size_t echo_size = 0;
    assert(shitnet_poll_tx(instance, echo.data(), echo.size(), &echo_size) ==
           1);
    assert(echo_size == echo.size());
    assert(echo[12] == 0x08 && echo[13] == 0x00);
    assert(echo[23] == 1);
    assert(echo[34] == 8 && echo[35] == 0);
    assert(echo[38] == 0x12 && echo[39] == 0x34);
    assert(echo[40] == 0x00 && echo[41] == 0x07);
    for (std::size_t i = 0; i < payload.size(); ++i)
        assert(echo[42 + i] == payload[i]);

    assert(shitnet_poll_event(instance, &event) == 0);
    shitnet_destroy(instance);
    return 0;
}
