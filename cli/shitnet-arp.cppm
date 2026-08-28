module;

#include <shitnet/shitnet.h>

#include <array>
#include <cstdint>
#include <cstdio>

export module shitnet.cli.arp;

namespace {

using Packet = std::array<std::uint8_t, 42>;

auto make_packet(std::uint16_t operation, std::uint8_t target) -> Packet {
    Packet packet{};

    for (std::size_t i = 0; i < 6; ++i)
        packet[i] = 0xff;

    packet[6] = 0xaa;
    packet[7] = 0xbb;
    packet[8] = 0xcc;
    packet[9] = 0xdd;
    packet[10] = 0xee;
    packet[11] = 0xff;
    packet[12] = 0x08;
    packet[13] = 0x06;
    packet[15] = 0x01;
    packet[16] = 0x08;
    packet[18] = 6;
    packet[19] = 4;
    packet[20] = static_cast<std::uint8_t>(operation >> 8);
    packet[21] = static_cast<std::uint8_t>(operation & 0xff);
    packet[22] = 0xaa;
    packet[23] = 0xbb;
    packet[24] = 0xcc;
    packet[25] = 0xdd;
    packet[26] = 0xee;
    packet[27] = 0xff;
    packet[28] = 10;
    packet[31] = 1;
    packet[38] = 10;
    packet[41] = target;
    return packet;
}

auto make_request(std::uint8_t target) -> Packet {
    return make_packet(1, target);
}

auto make_reply() -> Packet {
    auto packet = make_packet(2, 2);
    packet[0] = 0x02;
    packet[5] = 0x02;
    packet[32] = 0x02;
    packet[37] = 0x02;
    return packet;
}

auto dump_frame(const std::uint8_t *data, std::size_t length) -> void {
    for (std::size_t index = 0; index < length; ++index) {
        std::printf("%02x ", data[index]);
        if ((index + 1) % 16 == 0)
            std::printf("\n");
    }

    if (length % 16 != 0)
        std::printf("\n");
}

auto send_packet(shitnet *net, const Packet &packet) -> void {
    const auto receive_result =
        shitnet_receive(net, packet.data(), packet.size());
    std::printf("rx result=%d tx_queue=%zu\n", receive_result,
                shitnet_tx_size(net));

    std::array<std::uint8_t, 1500> output{};
    std::printf("tx queue: %zu\n", shitnet_tx_size(net));

    while (shitnet_tx_size(net) > 0) {
        std::size_t written = 0;
        const auto poll_result =
            shitnet_poll_tx(net, output.data(), output.size(), &written);
        std::printf("TX result=%d len=%zu\n", poll_result, written);
        if (poll_result == 1)
            dump_frame(output.data(), written);
    }
}

} // namespace

export namespace shitnet_cli::arp {

auto request(shitnet *net, int target_value) -> int {
    if (target_value < 0 || target_value > 255) {
        std::fprintf(stderr, "target must be between 0 and 255\n");
        return 1;
    }

    const auto target = static_cast<std::uint8_t>(target_value);
    std::printf("ARP request for 10.0.0.%u\n", static_cast<unsigned>(target));
    send_packet(net, make_request(target));
    return 0;
}

auto reply(shitnet *net) -> int {
    std::printf("ARP reply from 10.0.0.1\n");
    send_packet(net, make_reply());
    return 0;
}

auto learn(shitnet *net) -> int {
    const auto packet = make_reply();
    const auto receive_result =
        shitnet_receive(net, packet.data(), packet.size());
    if (receive_result < 0) {
        std::fprintf(stderr, "failed to receive ARP reply: %d\n",
                     receive_result);
        return 1;
    }

    const std::array<std::uint8_t, 4> ip{10, 0, 0, 1};
    std::array<std::uint8_t, 6> mac{};
    const auto lookup_result = shitnet_arp_lookup(net, ip.data(), mac.data());
    if (lookup_result != 1) {
        std::fprintf(stderr, "ARP lookup failed: %d\n", lookup_result);
        return 1;
    }

    std::printf("%u.%u.%u.%u -> %02x:%02x:%02x:%02x:%02x:%02x\n", ip[0], ip[1],
                ip[2], ip[3], mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 0;
}

auto bad_hardware(shitnet *net) -> int {
    auto packet = make_request(2);
    packet[14] = 0x12;
    packet[15] = 0x34;
    std::printf("unsupported ARP hardware\n");
    send_packet(net, packet);
    return 0;
}

auto bad_protocol(shitnet *net) -> int {
    auto packet = make_request(2);
    packet[16] = 0x86;
    packet[17] = 0xdd;
    std::printf("unsupported ARP protocol\n");
    send_packet(net, packet);
    return 0;
}

auto bad_length(shitnet *net) -> int {
    auto packet = make_request(2);
    packet[18] = 5;
    std::printf("invalid ARP address lengths\n");
    send_packet(net, packet);
    return 0;
}

auto bad_operation(shitnet *net) -> int {
    auto packet = make_packet(0x1337, 2);
    std::printf("unknown ARP operation\n");
    send_packet(net, packet);
    return 0;
}

} // namespace shitnet_cli::arp
