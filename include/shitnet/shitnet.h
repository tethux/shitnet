#ifndef SHITNET_SHITNET_H
#define SHITNET_SHITNET_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct shitnet shitnet;

shitnet *shitnet_create(void);
void shitnet_destroy(shitnet *instance);
int shitnet_receive(shitnet *instance, const uint8_t *data, size_t len);
size_t shitnet_tx_size(const shitnet *instance);

int shitnet_poll_tx(shitnet *instance, uint8_t *buffer, size_t buffer_size,
                    size_t *written);

#ifdef __cplusplus
}
#endif

#endif
