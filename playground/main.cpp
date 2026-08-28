#include <shitnet/shitnet.h>

#include <array>
#include <cstdint>
#include <cstdio>

int main() {
    std::array<std::uint8_t, 42> packet{};

    // Ethernet dst: broadcast
    packet[0] = 0xff;
    packet[1] = 0xff;
    packet[2] = 0xff;
    packet[3] = 0xff;
    packet[4] = 0xff;
    packet[5] = 0xff;

    // Ethernet src: requester MAC
    packet[6] = 0xaa;
    packet[7] = 0xbb;
    packet[8] = 0xcc;
    packet[9] = 0xdd;
    packet[10] = 0xee;
    packet[11] = 0xff;

    // EtherType = ARP
    packet[12] = 0x08;
    packet[13] = 0x06;

    // ARP hardware type = Ethernet
    packet[14] = 0x00;
    packet[15] = 0x01;

    // ARP protocol type = IPv4
    packet[16] = 0x08;
    packet[17] = 0x00;

    // MAC len / IP len
    packet[18] = 6;
    packet[19] = 4;

    // ARP request
    packet[20] = 0x00;
    packet[21] = 0x01;

    // sender MAC
    packet[22] = 0xaa;
    packet[23] = 0xbb;
    packet[24] = 0xcc;
    packet[25] = 0xdd;
    packet[26] = 0xee;
    packet[27] = 0xff;

    // sender IP = 10.0.0.1
    packet[28] = 10;
    packet[29] = 0;
    packet[30] = 0;
    packet[31] = 1;

    // target MAC is unknown => zeroes already

    // target IP = 10.0.0.2
    packet[38] = 10;
    packet[39] = 0;
    packet[40] = 0;
    packet[41] = 2;

    shitnet *net = shitnet_create();
    if (net == nullptr) {
        return 1;
    }

    const int result = shitnet_receive(net, packet.data(), packet.size());

    std::printf("receive result = %d\n", result);
    std::printf("tx queue size = %zu\n", shitnet_tx_size(net));

    shitnet_destroy(net);
}
