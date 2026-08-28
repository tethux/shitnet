#include <shitnet/shitnet.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <shitnet/macros.h>
#include <string_view>

using Packet = std::array<std::uint8_t, 42>;

static fn make_arp_packet(std::uint16_t operation,
                          std::uint8_t target_last_octet) -> Packet {
    Packet packet{};

    // Ethernet dst = broadcast
    for (std::size_t i = 0; i < 6; ++i)
        packet[i] = 0xff;

    // Ethernet src = aa:bb:cc:dd:ee:ff
    packet[6] = 0xaa;
    packet[7] = 0xbb;
    packet[8] = 0xcc;
    packet[9] = 0xdd;
    packet[10] = 0xee;
    packet[11] = 0xff;

    // EtherType = ARP
    packet[12] = 0x08;
    packet[13] = 0x06;

    // Hardware type = Ethernet
    packet[14] = 0x00;
    packet[15] = 0x01;

    // Protocol type = IPv4
    packet[16] = 0x08;
    packet[17] = 0x00;

    // Hardware length = 6
    packet[18] = 6;

    // Protocol length = 4
    packet[19] = 4;

    // Operation
    packet[20] = static_cast<std::uint8_t>(operation >> 8);
    packet[21] = static_cast<std::uint8_t>(operation & 0xff);

    // Sender MAC = aa:bb:cc:dd:ee:ff
    packet[22] = 0xaa;
    packet[23] = 0xbb;
    packet[24] = 0xcc;
    packet[25] = 0xdd;
    packet[26] = 0xee;
    packet[27] = 0xff;

    // Sender IP = 10.0.0.1
    packet[28] = 10;
    packet[29] = 0;
    packet[30] = 0;
    packet[31] = 1;

    // Target MAC
    //
    // For requests this is normally zero.
    // For replies we'll fill this later if needed.

    // Target IP = 10.0.0.X
    packet[38] = 10;
    packet[39] = 0;
    packet[40] = 0;
    packet[41] = target_last_octet;

    return packet;
}

static fn make_arp_request(std::uint8_t target_last_octet) -> Packet {
    return make_arp_packet(1, target_last_octet);
}

static fn make_arp_reply() -> Packet {
    let packet = make_arp_packet(2, 2);

    // Ethernet dst = our shitnet MAC
    packet[0] = 0x02;
    packet[1] = 0x00;
    packet[2] = 0x00;
    packet[3] = 0x00;
    packet[4] = 0x00;
    packet[5] = 0x02;

    // ARP target MAC = our shitnet MAC
    packet[32] = 0x02;
    packet[33] = 0x00;
    packet[34] = 0x00;
    packet[35] = 0x00;
    packet[36] = 0x00;
    packet[37] = 0x02;

    return packet;
}

static fn dump_frame(const std::uint8_t *data, std::size_t len) -> void {
    for (std::size_t i = 0; i < len; ++i) {
        std::printf("%02x ", data[i]);

        if ((i + 1) % 16 == 0)
            std::printf("\n");
    }

    if (len % 16 != 0)
        std::printf("\n");
}

static fn drain_tx(shitnet *net) -> void {
    std::array<std::uint8_t, 1500> output{};

    std::printf("tx queue: %zu\n", shitnet_tx_size(net));

    while (shitnet_tx_size(net) > 0) {
        std::size_t written = 0;

        const let result =
            shitnet_poll_tx(net, output.data(), output.size(), &written);

        std::printf("TX result=%d len=%zu\n", result, written);

        if (result == 1)
            dump_frame(output.data(), written);
    }
}

static fn send_packet(shitnet *net, const Packet &packet) -> void {
    const let result = shitnet_receive(net, packet.data(), packet.size());

    std::printf("rx result=%d tx_queue=%zu\n", result, shitnet_tx_size(net));

    drain_tx(net);
}

static fn learn_arp(shitnet *net) -> int {
    const let packet = make_arp_reply();
    const let receive_result =
        shitnet_receive(net, packet.data(), packet.size());

    if (receive_result < 0) {
        std::fprintf(stderr, "failed to receive ARP reply: %d\n",
                     receive_result);
        return 1;
    }

    const std::array<std::uint8_t, 4> ip{10, 0, 0, 1};
    std::array<std::uint8_t, 6> mac{};
    const let lookup_result = shitnet_arp_lookup(net, ip.data(), mac.data());

    if (lookup_result != 1) {
        std::fprintf(stderr, "ARP lookup failed: %d\n", lookup_result);
        return 1;
    }

    std::printf("%u.%u.%u.%u -> %02x:%02x:%02x:%02x:%02x:%02x\n", ip[0], ip[1],
                ip[2], ip[3], mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 0;
}

static fn usage(const char *program) -> void {
    std::printf("usage:\n"
                "  %s arp request <target-last-octet>\n"
                "  %s arp reply\n"
                "  %s arp learn\n"
                "  %s arp bad-hardware\n"
                "  %s arp bad-protocol\n"
                "  %s arp bad-length\n"
                "  %s arp bad-operation\n"
                "\n"
                "examples:\n"
                "  %s arp request 2\n"
                "  %s arp request 99\n"
                "  %s arp reply\n"
                "  %s arp learn\n"
                "  %s arp bad-hardware\n",
                program, program, program, program, program, program, program,
                program, program, program, program, program);
}

fn main(int argc, char **argv) -> int {
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    const std::string_view protocol{argv[1]};
    const std::string_view command{argv[2]};

    if (protocol != "arp") {
        usage(argv[0]);
        return 1;
    }

    shitnet *net = shitnet_create();

    if (net == nullptr) {
        std::fprintf(stderr, "failed to create shitnet\n");

        return 1;
    }

    if (command == "request") {
        if (argc < 4) {
            usage(argv[0]);
            shitnet_destroy(net);
            return 1;
        }

        const let target =
            static_cast<std::uint8_t>(std::strtoul(argv[3], nullptr, 10));

        std::printf("ARP request for 10.0.0.%u\n",
                    static_cast<unsigned>(target));

        const let packet = make_arp_request(target);

        send_packet(net, packet);
    }

    else if (command == "reply") {
        std::printf("ARP reply from 10.0.0.1\n");

        const let packet = make_arp_reply();

        send_packet(net, packet);
    }

    else if (command == "learn") {
        const let result = learn_arp(net);

        shitnet_destroy(net);
        return result;
    }

    else if (command == "bad-hardware") {
        let packet = make_arp_request(2);

        // ARP hardware type 0x1234
        packet[14] = 0x12;
        packet[15] = 0x34;

        std::printf("unsupported ARP hardware\n");

        send_packet(net, packet);
    }

    else if (command == "bad-protocol") {
        let packet = make_arp_request(2);

        // ARP protocol type = IPv6 EtherType
        packet[16] = 0x86;
        packet[17] = 0xdd;

        std::printf("unsupported ARP protocol\n");

        send_packet(net, packet);
    }

    else if (command == "bad-length") {
        let packet = make_arp_request(2);

        packet[18] = 5;
        packet[19] = 4;

        std::printf("invalid ARP address lengths\n");

        send_packet(net, packet);
    }

    else if (command == "bad-operation") {
        let packet = make_arp_packet(0x1337, 2);

        std::printf("unknown ARP operation\n");

        send_packet(net, packet);
    }

    else {
        std::printf("unknown command: %s\n", argv[2]);

        usage(argv[0]);

        shitnet_destroy(net);
        return 1;
    }

    shitnet_destroy(net);
    return 0;
}
