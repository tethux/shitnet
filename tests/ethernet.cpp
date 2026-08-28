#include <shitnet/shitnet.h>

#include <cassert>

int main() {
    shitnet *instance = shitnet_create();
    assert(instance != nullptr);
    shitnet_destroy(instance);
    return 0;
}
