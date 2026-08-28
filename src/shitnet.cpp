#include <cstddef>
#include <cstdint>
#include <shitnet/shitnet.h>

#include <deque>
#include <span>
#include <utility>
#include <vector>

import shitnet.address;
import shitnet.arp;
import shitnet.ethernet;
import shitnet.frame;
import shitnet.icmp;
import shitnet.ipv4;
import shitnet.match;

struct shitnet {
    MacAddress mac{
        .bytes =
            {
                std::byte{0x02},
                std::byte{0x00},
                std::byte{0x00},
                std::byte{0x00},
                std::byte{0x00},
                std::byte{0x02},
            },
    };

    IPv4Address ip{
        .bytes =
            {
                std::byte{10},
                std::byte{0},
                std::byte{0},
                std::byte{2},
            },
    };

    std::vector<ArpEntry> arp_table;
    std::deque<Frame> tx;
};

static auto internet_checksum(std::span<const std::byte> bytes)
    -> std::uint16_t {
    std::uint32_t sum = 0;
    std::size_t i = 0;

    while (i + 1 < bytes.size()) {
        const auto hi = std::to_integer<std::uint16_t>(bytes[i]);

        const auto lo = std::to_integer<std::uint16_t>(bytes[i + 1]);

        sum += static_cast<std::uint16_t>((hi << 8) | lo);

        i += 2;
    }

    if (i < bytes.size()) {
        sum += static_cast<std::uint16_t>(
            std::to_integer<std::uint16_t>(bytes[i]) << 8);
    }

    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return static_cast<std::uint16_t>(~sum);
}

static auto write_u16(std::span<std::byte> bytes, std::size_t offset,
                      std::uint16_t value) -> void {
    bytes[offset] = std::byte{static_cast<std::uint8_t>(value >> 8)};

    bytes[offset + 1] = std::byte{static_cast<std::uint8_t>(value & 0xff)};
}

static auto make_arp_reply(const shitnet &net, const ArpPacketView &req)
    -> Frame {
    Frame reply{42};
    auto bytes = reply.bytes();

    const auto req_mac = req.senderMac();
    const auto req_ip = req.senderIp();

    for (std::size_t i = 0; i < 6; ++i)
        bytes[i] = req_mac.bytes[i];

    for (std::size_t i = 0; i < 6; ++i)
        bytes[6 + i] = net.mac.bytes[i];

    bytes[12] = std::byte{0x08};
    bytes[13] = std::byte{0x06};

    bytes[14] = std::byte{0x00};
    bytes[15] = std::byte{0x01};

    bytes[16] = std::byte{0x08};
    bytes[17] = std::byte{0x00};

    bytes[18] = std::byte{0x06};
    bytes[19] = std::byte{0x04};

    bytes[20] = std::byte{0x00};
    bytes[21] = std::byte{0x02};

    for (std::size_t i = 0; i < 6; ++i)
        bytes[22 + i] = net.mac.bytes[i];

    for (std::size_t i = 0; i < 4; ++i)
        bytes[28 + i] = net.ip.bytes[i];

    for (std::size_t i = 0; i < 6; ++i)
        bytes[32 + i] = req_mac.bytes[i];

    for (std::size_t i = 0; i < 4; ++i)
        bytes[38 + i] = req_ip.bytes[i];

    return reply;
}

static auto make_icmp_echo_reply(const shitnet &net,
                                 const EthernetFrameView &ethernet,
                                 const IPv4PacketView &ip,
                                 const IcmpPacketView &icmp) -> Frame {
    const auto icmp_size = 8 + icmp.payload().size();

    const auto ipv4_size = 20 + icmp_size;

    Frame reply{14 + ipv4_size};

    auto bytes = reply.bytes();

    // ethernet

    const auto destination_mac = ethernet.source();

    for (std::size_t i = 0; i < 6; ++i)
        bytes[i] = destination_mac.bytes[i];

    for (std::size_t i = 0; i < 6; ++i)
        bytes[6 + i] = net.mac.bytes[i];

    bytes[12] = std::byte{0x08};
    bytes[13] = std::byte{0x00};

    // IPv4 start at byte 14

    const auto ip_offset = std::size_t{14};

    bytes[ip_offset + 0] = std::byte{0x45};

    bytes[ip_offset + 1] = std::byte{0x00};

    write_u16(bytes, ip_offset + 2, static_cast<std::uint16_t>(ipv4_size));

    // identification
    write_u16(bytes, ip_offset + 4, 0);

    // flags + fragment offset
    write_u16(bytes, ip_offset + 6, 0);

    // TTL
    bytes[ip_offset + 8] = std::byte{64};

    // protocol = imcp
    bytes[ip_offset + 9] =
        std::byte{static_cast<std::uint8_t>(IpProtocol::icmp)};

    // checksum must be zero while calculating it
    bytes[ip_offset + 10] = std::byte{0};

    bytes[ip_offset + 11] = std::byte{0};

    // source = wahtever instance has rn
    for (std::size_t i = 0; i < 4; ++i)
        bytes[ip_offset + 12 + i] = net.ip.bytes[i];

    // dst
    const auto destination_ip = ip.source();

    for (std::size_t i = 0; i < 4; ++i)
        bytes[ip_offset + 16 + i] = destination_ip.bytes[i];

    // imcmp start after 20-byte ipv4 header

    const auto icmp_offset = ip_offset + 20;

    // echo reply
    bytes[icmp_offset + 0] =
        std::byte{static_cast<std::uint8_t>(IcmpType::echoReply)};

    bytes[icmp_offset + 1] = std::byte{0};

    // checksum = 0 temporarily
    bytes[icmp_offset + 2] = std::byte{0};

    bytes[icmp_offset + 3] = std::byte{0};

    // same identifier
    write_u16(bytes, icmp_offset + 4, icmp.identifier());

    // same sequence number
    write_u16(bytes, icmp_offset + 6, icmp.sequence());

    // same payload
    const auto payload = icmp.payload();

    for (std::size_t i = 0; i < payload.size(); ++i) {
        bytes[icmp_offset + 8 + i] = payload[i];
    }

    // icmp checksum

    const auto icmp_bytes = std::span<const std::byte>{
        bytes.data() + icmp_offset,
        icmp_size,
    };

    const auto icmp_checksum = internet_checksum(icmp_bytes);

    write_u16(bytes, icmp_offset + 2, icmp_checksum);

    // ipv4 checksum of the ipv4 header

    const auto ip_header = std::span<const std::byte>{
        bytes.data() + ip_offset,
        20,
    };

    const auto ip_checksum = internet_checksum(ip_header);

    write_u16(bytes, ip_offset + 10, ip_checksum);

    return reply;
}

static auto learn_arp(shitnet &net, IPv4Address ip, MacAddress mac) -> void {
    for (auto &entry : net.arp_table) {
        if (entry.ip == ip) {
            entry.mac = mac;
            return;
        }
    }

    net.arp_table.push_back(ArpEntry{
        .ip = ip,
        .mac = mac,
    });
}

static auto handle_arp(shitnet &net, ArpPacket packet) -> int {
    return match(
        std::move(packet), case_of<ArpRequest>([&](const auto &req) {
            learn_arp(net, req.packet.senderIp(), req.packet.senderMac());

            if (req.packet.targetIp() != net.ip)
                return 0;

            auto reply = make_arp_reply(net, req.packet);

            net.tx.push_back(std::move(reply));

            return 1;
        }),

        case_of<ArpReply>([&](const auto &reply) {
            learn_arp(net, reply.packet.senderIp(), reply.packet.senderMac());

            return 0;
        }),

        case_of<UnsupportedArpHardware>([](const auto &) { return 0; }),
        case_of<UnsupportedArpProtocol>([](const auto &) { return 0; }),
        case_of<InvalidArpAddressLengths>([](const auto &) { return -2; }),
        case_of<UnknownArpOperation>([](const auto &) { return 0; }));
}

static auto handle_icmp(shitnet &net, const EthernetFrameView &ethernet,
                        const IPv4PacketView &ip, IcmpPacket packet) -> int {
    return match(std::move(packet),
                 case_of<IcmpEchoRequest>([&](const auto &request) {
                     auto reply = make_icmp_echo_reply(net, ethernet, ip,
                                                       request.packet);

                     net.tx.push_back(std::move(reply));

                     return 1;
                 }),
                 case_of<IcmpEchoReply>([](const auto &) { return 0; }),
                 case_of<UnsupportedIcmpType>([](const auto &) { return 0; }),
                 case_of<UnsupportedIcmpCode>([](const auto &) { return 0; }));
}

static auto handle_ipv4(shitnet &net, const EthernetFrameView &ethernet,
                        IPv4Packet packet) -> int {
    return match(
        std::move(packet), case_of<IPv4Icmp>([&](const auto &icmp_ip) {
            const auto &ip = icmp_ip.packet;

            if (ip.destination() != net.ip)
                return 0;

            const auto payload = ip.payload();

            if (payload.size() < 8)
                return -2;

            const IcmpPacketView icmp{payload};

            return handle_icmp(net, ethernet, ip, classifyIcmp(icmp));
        }),

        case_of<IPv4Tcp>([](const auto &) {
            // future veyas issue
            return 0;
        }),

        case_of<IPv4Udp>([](const auto &) {
            // future veyas issue
            return 0;
        }),
        case_of<UnsupportedIpVersion>([](const auto &) { return 0; }),
        case_of<UnsupportedIpv4HeaderLength>([](const auto &) { return 0; }),
        case_of<InvalidIpv4Length>([](const auto &) { return -2; }),
        case_of<UnknownIpProtocol>([](const auto &) { return 0; }));
}

extern "C" auto shitnet_create(void) -> shitnet * {
    try {
        return new shitnet{};
    } catch (...) {
        return nullptr;
    }
}

extern "C" auto shitnet_destroy(shitnet *instance) -> void {
    try {
        delete instance;
    } catch (...) {
    }
}

extern "C" auto shitnet_tx_size(const shitnet *instance) -> size_t {
    if (instance == nullptr)
        return 0;

    return instance->tx.size();
}

extern "C" auto shitnet_arp_lookup(const shitnet *instance, const uint8_t ip[4],
                                   uint8_t mac[6]) -> int {
    try {
        if (instance == nullptr || ip == nullptr || mac == nullptr) {
            return -1;
        }

        IPv4Address address{};

        for (std::size_t i = 0; i < address.bytes.size(); ++i) {
            address.bytes[i] = std::byte{ip[i]};
        }

        for (const auto &entry : instance->arp_table) {
            if (entry.ip != address)
                continue;

            for (std::size_t i = 0; i < entry.mac.bytes.size(); ++i) {
                mac[i] = std::to_integer<std::uint8_t>(entry.mac.bytes[i]);
            }

            return 1;
        }

        return 0;

    } catch (...) {
        return -1;
    }
}

extern "C" auto shitnet_poll_tx(shitnet *instance, uint8_t *buffer,
                                size_t buffer_size, size_t *written) -> int {
    try {
        if (instance == nullptr || buffer == nullptr || written == nullptr) {
            return -1;
        }

        if (instance->tx.empty()) {
            *written = 0;
            return 0;
        }

        const auto &frame = instance->tx.front();

        const auto bytes = frame.bytes();

        if (buffer_size < bytes.size())
            return -2;

        for (std::size_t i = 0; i < bytes.size(); ++i) {
            buffer[i] = std::to_integer<std::uint8_t>(bytes[i]);
        }

        *written = bytes.size();

        instance->tx.pop_front();

        return 1;

    } catch (...) {
        return -1;
    }
}

extern "C" auto shitnet_receive(shitnet *instance, const uint8_t *data,
                                size_t len) -> int {
    try {
        if (instance == nullptr || data == nullptr || len < 14) {
            return -1;
        }

        const auto bytes = std::span{
            reinterpret_cast<const std::byte *>(data),
            len,
        };

        const EthernetFrameView frame{bytes};

        switch (frame.etherType()) {
        case EtherType::arp: {
            const auto payload = frame.payload();

            if (payload.size() < 28)
                return -2;

            const ArpPacketView arp{payload};

            return handle_arp(*instance, classifyArp(arp));
        }

        case EtherType::ipv4: {
            const auto payload = frame.payload();

            if (payload.size() < 20)
                return -2;

            const IPv4PacketView ip{payload};

            return handle_ipv4(*instance, frame,
                               classifyIPv4(ip, payload.size()));
        }

        default:
            return 0;
        }

    } catch (...) {
        return -1;
    }
}
