#include <cstddef>
#include <cstdint>
#include <shitnet/macros.h>
#include <shitnet/shitnet.h>

#include <deque>
#include <span>
#include <utility>
#include <vector>

import shitnet.arp;
import shitnet.frame;
import shitnet.ethernet;

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

static fn make_arp_reply(const shitnet &net, const ArpPacketView &req)
    -> Frame {
    Frame reply{42};
    let bytes = reply.bytes();

    const let req_mac = req.senderMac();
    const let req_ip = req.senderIp();

    // ethernet dst
    for (std::size_t i = 0; i < 6; ++i) {
        bytes[i] = req_mac.bytes[i];
    }

    // ethernet src
    for (std::size_t i = 0; i < 6; ++i)
        bytes[6 + i] = net.mac.bytes[i];

    // ethertype = arp
    bytes[12] = std::byte{0x08};
    bytes[13] = std::byte{0x06};

    // arp: ethernet
    bytes[14] = std::byte{0x00};
    bytes[15] = std::byte{0x01};

    // arp: ipv4
    bytes[16] = std::byte{0x08};
    bytes[17] = std::byte{0x00};

    // mac len, ip len
    bytes[18] = std::byte{0x06};
    bytes[19] = std::byte{0x04};

    // operation = reply
    bytes[20] = std::byte{0x00};
    bytes[21] = std::byte{0x02};

    // sender mac = use the vals from net net obj
    for (std::size_t i = 0; i < 6; ++i)
        bytes[22 + i] = net.mac.bytes[i];

    // sender ip = use the vals from net net obj
    for (std::size_t i = 0; i < 4; ++i)
        bytes[28 + i] = net.ip.bytes[i];

    // target mac = requester
    for (std::size_t i = 0; i < 6; ++i)
        bytes[32 + i] = req_mac.bytes[i];

    // target ip = requester
    for (std::size_t i = 0; i < 4; ++i)
        bytes[38 + i] = req_ip.bytes[i];

    return reply;
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
        if (instance == nullptr || ip == nullptr || mac == nullptr)
            return -1;

        IPv4Address address{};

        for (std::size_t i = 0; i < address.bytes.size(); ++i)
            address.bytes[i] = std::byte{ip[i]};

        for (const let &entry : instance->arp_table) {
            if (entry.ip != address)
                continue;

            for (std::size_t i = 0; i < entry.mac.bytes.size(); ++i)
                mac[i] = std::to_integer<std::uint8_t>(entry.mac.bytes[i]);

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
        if (instance == nullptr || buffer == nullptr || written == nullptr)
            return -1;

        if (instance->tx.empty()) {
            *written = 0;
            return 0;
        }

        const let &frame = instance->tx.front();
        const let bytes = frame.bytes();

        if (buffer_size < bytes.size()) {
            return -2;
        }

        for (std::size_t i = 0; i < bytes.size(); ++i)
            buffer[i] = std::to_integer<std::uint8_t>(bytes[i]);

        *written = bytes.size();

        instance->tx.pop_front();
        return 1;

    } catch (...) {
        return -1;
    }
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
            if (req.packet.targetIp() != net.ip) {
                return 0;
            }

            let reply = make_arp_reply(net, req.packet);
            net.tx.push_back(std::move(reply));

            return 1;
        },

        case_(ArpReply, reply) {
            (void)reply;

            learn_arp(net, reply.packet.senderIp(), reply.packet.senderMac());

            return 0;
        },

        case_(UnsupportedArpHardware, bad) {
            (void)bad;
            return 0;
        },

        case_(UnsupportedArpProtocol, bad) {
            (void)bad;
            return 0;
        },

        case_(InvalidArpAddressLengths, bad) {
            (void)bad;
            return -2;
        },

        case_(UnknownArpOperation, bad) {
            (void)bad;
            return 0;
        });
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

        if (frame.etherType() != EtherType::arp) {
            return 0;
        }

        if (frame.payload().size() < 28) {
            return -2;
        }

        const ArpPacketView arp{frame.payload()};

        return handle_arp(*instance, classifyArp(arp));
    } catch (...) {
        return -1;
    }
}
