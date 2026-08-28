#include <shitnet/shitnet.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

static std::array<std::uint8_t, 42>
make_arp_request(std::uint8_t target_last_octet) {
    std::array<std::uint8_t, 42> packet{};

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

    // Protocol = IPv4
    packet[16] = 0x08;
    packet[17] = 0x00;

    // MAC length / IP length
    packet[18] = 6;
    packet[19] = 4;

    // Operation = request
    packet[20] = 0x00;
    packet[21] = 0x01;

    // Sender MAC
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

    // Target MAC = unknown, stays 00:00:00:00:00:00

    // Target IP = 10.0.0.X
    packet[38] = 10;
    packet[39] = 0;
    packet[40] = 0;
    packet[41] = target_last_octet;

    return packet;
}

static void dump_frame(const std::uint8_t *data, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) {
        std::printf("%02x ", data[i]);

        if ((i + 1) % 16 == 0)
            std::printf("\n");
    }

    if (len % 16 != 0)
        std::printf("\n");
}

static void usage(const char *program) {
    std::printf("usage:\n"
                "  %s arp <target-last-octet> [count]\n"
                "\n"
                "examples:\n"
                "  %s arp 2\n"
                "  %s arp 2 10\n"
                "  %s arp 99 5\n",
                program, program, program, program);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    const std::string_view command{argv[1]};

    if (command != "arp") {
        std::printf("unknown command: %s\n", argv[1]);
        usage(argv[0]);
        return 1;
    }

    const auto target =
        static_cast<std::uint8_t>(std::strtoul(argv[2], nullptr, 10));

    std::size_t count = 1;

    if (argc >= 4)
        count = std::strtoul(argv[3], nullptr, 10);

    shitnet *net = shitnet_create();

    if (net == nullptr) {
        std::fprintf(stderr, "failed to create shitnet\n");
        return 1;
    }

    std::printf("sending %zu ARP request(s) for 10.0.0.%u\n\n", count,
                static_cast<unsigned>(target));

    for (std::size_t i = 0; i < count; ++i) {
        auto packet = make_arp_request(target);

        const int result = shitnet_receive(net, packet.data(), packet.size());

        std::printf("[%zu] rx result=%d tx_queue=%zu\n", i, result,
                    shitnet_tx_size(net));
    }

    std::printf("\nqueued replies: %zu\n", shitnet_tx_size(net));

    std::array<std::uint8_t, 1500> output{};

    while (shitnet_tx_size(net) > 0) {
        std::size_t written = 0;

        const int result =
            shitnet_poll_tx(net, output.data(), output.size(), &written);

        std::printf("\nTX frame: result=%d len=%zu\n", result, written);

        if (result == 1)
            dump_frame(output.data(), written);
    }

    shitnet_destroy(net);
}
