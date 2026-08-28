#include <shitnet/shitnet.h>

#include <span>

import shitnet.arp;
import shitnet.ethernet;

struct shitnet {
    int dummy = 0;
};

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
        return arp.operation() == 1 ? 1 : 0;
    } catch (...) {
        return -1;
    }
}
