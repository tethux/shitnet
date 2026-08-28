#include <shitnet/shitnet.h>

#include <deque>
#include <span>
#include <utility>

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
    std::deque<Frame> tx;
};

static Frame make_arp_reply(const shitnet &net, const ArpPacketView &req) {
    Frame reply{42};
    auto bytes = reply.bytes();

    const auto req_mac = req.senderMac();
    const auto req_ip = req.senderIp();

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

extern "C" shitnet *shitnet_create(void) {
    try {
        return new shitnet{};
    } catch (...) {
        return nullptr;
    }
}

extern "C" void shitnet_destroy(shitnet *instance) {
    try {
        delete instance;
    } catch (...) {
    }
}

extern "C" int shitnet_receive(shitnet *instance, const uint8_t *data,
                               size_t len) {
    try {
        if (instance == nullptr || data == nullptr || len < 14) {
            return -1;
        }

        const auto bytes = std::span{
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

        if (arp.operation() != ArpOperation::request) {
            return 0;
        }

        if (arp.targetIp() != instance->ip) {
            return 0;
        }

        auto reply = make_arp_reply(*instance, arp);
        instance->tx.push_back(std::move(reply));
        return 1;
    } catch (...) {
        return -1;
    }
}

extern "C" size_t shitnet_tx_size(const shitnet *instance) {
    if (instance == nullptr)
        return 0;

    return instance->tx.size();
}
