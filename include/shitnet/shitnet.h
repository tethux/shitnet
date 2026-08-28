#ifndef SHITNET_SHITNET_H
#define SHITNET_SHITNET_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct shitnet shitnet;

shitnet *shitnet_create(void);
void shitnet_destroy(shitnet *instance);

#ifdef __cplusplus
}
#endif

#endif
