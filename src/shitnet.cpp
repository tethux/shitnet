#include <cstddef>
#include <cstdint>
#include <shitnet/macros.h>
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

static fn internet_checksum(std::span<const std::byte> bytes) -> std::uint16_t {
    std::uint32_t sum = 0;
    std::size_t i = 0;

    while (i + 1 < bytes.size()) {
        const let hi = std::to_integer<std::uint16_t>(bytes[i]);

        const let lo = std::to_integer<std::uint16_t>(bytes[i + 1]);

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

static fn write_u16(std::span<std::byte> bytes, std::size_t offset,
                    std::uint16_t value) -> void {
    bytes[offset] = std::byte{static_cast<std::uint8_t>(value >> 8)};

    bytes[offset + 1] = std::byte{static_cast<std::uint8_t>(value & 0xff)};
}

static fn make_arp_reply(const shitnet &net, const ArpPacketView &req)
    -> Frame {
    Frame reply{42};
    let bytes = reply.bytes();

    const let req_mac = req.senderMac();
    const let req_ip = req.senderIp();

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

static fn make_icmp_echo_reply(const shitnet &net,
                               const EthernetFrameView &ethernet,
                               const IPv4PacketView &ip,
                               const IcmpPacketView &icmp) -> Frame {
    const let icmp_size = 8 + icmp.payload().size();

    const let ipv4_size = 20 + icmp_size;

    Frame reply{14 + ipv4_size};

    let bytes = reply.bytes();

    // ethernet

    const let destination_mac = ethernet.source();

    for (std::size_t i = 0; i < 6; ++i)
        bytes[i] = destination_mac.bytes[i];

    for (std::size_t i = 0; i < 6; ++i)
        bytes[6 + i] = net.mac.bytes[i];

    bytes[12] = std::byte{0x08};
    bytes[13] = std::byte{0x00};

    // IPv4 start at byte 14

    const let ip_offset = std::size_t{14};

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
    const let destination_ip = ip.source();

    for (std::size_t i = 0; i < 4; ++i)
        bytes[ip_offset + 16 + i] = destination_ip.bytes[i];

    // imcmp start after 20-byte ipv4 header

    const let icmp_offset = ip_offset + 20;

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
    const let payload = icmp.payload();

    for (std::size_t i = 0; i < payload.size(); ++i) {
        bytes[icmp_offset + 8 + i] = payload[i];
    }

    // icmp checksum

    const let icmp_bytes = std::span<const std::byte>{
        bytes.data() + icmp_offset,
        icmp_size,
    };

    const let icmp_checksum = internet_checksum(icmp_bytes);

    write_u16(bytes, icmp_offset + 2, icmp_checksum);

    // ipv4 checksum of the ipv4 header

    const let ip_header = std::span<const std::byte>{
        bytes.data() + ip_offset,
        20,
    };

    const let ip_checksum = internet_checksum(ip_header);

    write_u16(bytes, ip_offset + 10, ip_checksum);

    return reply;
}

static fn learn_arp(shitnet &net, IPv4Address ip, MacAddress mac) -> void {
    for (let &entry : net.arp_table) {
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

static fn handle_arp(shitnet &net, ArpPacket packet) -> int {
    return match(std::move(packet))(
        case_(ArpRequest, req) {
            learn_arp(net, req.packet.senderIp(), req.packet.senderMac());

            if (req.packet.targetIp() != net.ip)
                return 0;

            let reply = make_arp_reply(net, req.packet);

            net.tx.push_back(std::move(reply));

            return 1;
        },

        case_(ArpReply, reply) {
            learn_arp(net, reply.packet.senderIp(), reply.packet.senderMac());

            return 0;
        },

        case_(UnsupportedArpHardware) { return 0; },

        case_(UnsupportedArpProtocol) { return 0; },

        case_(InvalidArpAddressLengths) { return -2; },

        case_(UnknownArpOperation) { return 0; });
}

static fn handle_icmp(shitnet &net, const EthernetFrameView &ethernet,
                      const IPv4PacketView &ip, IcmpPacket packet) -> int {
    return match(std::move(packet))(
        case_(IcmpEchoRequest, request) {
            let reply = make_icmp_echo_reply(net, ethernet, ip, request.packet);

            net.tx.push_back(std::move(reply));

            return 1;
        },

        case_(IcmpEchoReply) { return 0; },

        case_(UnsupportedIcmpType) { return 0; },

        case_(UnsupportedIcmpCode) { return 0; });
}

static fn handle_ipv4(shitnet &net, const EthernetFrameView &ethernet,
                      IPv4Packet packet) -> int {
    return match(std::move(packet))(
        case_(IPv4Icmp, icmp_ip) {
            const let &ip = icmp_ip.packet;

            if (ip.destination() != net.ip)
                return 0;

            const let payload = ip.payload();

            if (payload.size() < 8)
                return -2;

            const IcmpPacketView icmp{payload};

            return handle_icmp(net, ethernet, ip, classifyIcmp(icmp));
        },

        case_(IPv4Tcp) {
            // future veyas issue
            return 0;
        },

        case_(IPv4Udp) {
            // future veyas issue
            return 0;
        },

        case_(UnsupportedIpVersion) { return 0; },

        case_(UnsupportedIpv4HeaderLength) { return 0; },

        case_(InvalidIpv4Length) { return -2; },

        case_(UnknownIpProtocol) { return 0; });
}

cfn shitnet_create(void) -> shitnet * {
    try {
        return new shitnet{};
    } catch (...) {
        return nullptr;
    }
}

cfn shitnet_destroy(shitnet *instance) -> void {
    try {
        delete instance;
    } catch (...) {
    }
}

cfn shitnet_tx_size(const shitnet *instance) -> size_t {
    if (instance == nullptr)
        return 0;

    return instance->tx.size();
}

cfn shitnet_arp_lookup(const shitnet *instance, const uint8_t ip[4],
                       uint8_t mac[6]) -> int {
    try {
        if (instance == nullptr || ip == nullptr || mac == nullptr) {
            return -1;
        }

        IPv4Address address{};

        for (std::size_t i = 0; i < address.bytes.size(); ++i) {
            address.bytes[i] = std::byte{ip[i]};
        }

        for (const let &entry : instance->arp_table) {
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

cfn shitnet_poll_tx(shitnet *instance, uint8_t *buffer, size_t buffer_size,
                    size_t *written) -> int {
    try {
        if (instance == nullptr || buffer == nullptr || written == nullptr) {
            return -1;
        }

        if (instance->tx.empty()) {
            *written = 0;
            return 0;
        }

        const let &frame = instance->tx.front();

        const let bytes = frame.bytes();

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

cfn shitnet_receive(shitnet *instance, const uint8_t *data, size_t len) -> int {
    try {
        if (instance == nullptr || data == nullptr || len < 14) {
            return -1;
        }

        const let bytes = std::span{
            reinterpret_cast<const std::byte *>(data),
            len,
        };

        const EthernetFrameView frame{bytes};

        switch (frame.etherType()) {
        case EtherType::arp: {
            const let payload = frame.payload();

            if (payload.size() < 28)
                return -2;

            const ArpPacketView arp{payload};

            return handle_arp(*instance, classifyArp(arp));
        }

        case EtherType::ipv4: {
            const let payload = frame.payload();

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
