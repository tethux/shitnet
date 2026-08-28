#include "shitnet.hpp"

#include <shitnet/shitnet.h>

struct shitnet {
    shitnet_private::Shitnet implementation;
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
