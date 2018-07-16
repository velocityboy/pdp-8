/**
 * IOT dispatch for RK8E: controller for RK05
 */
 #include <stdlib.h>
 
#include "pdp8/devices.h"

static const int RK_DEVICE_ID = 074;

static int rk_install(pdp8_device_t *dev, pdp8_t *pdp8);
static void rk_reset(pdp8_device_t *dev);
static void rk_dispatch(pdp8_device_t *dev, pdp8_t *pdp8, uint12_t opword);
static void rk_free(pdp8_device_t *dev);


struct pdp8_rk8e_t {
    pdp8_device_t device;
    pdp8_t *pdp8;

    uint32_t intr_bit;

    pdp8_rk8e_callbacks_t callbacks;
};

int pdp8_install_rk8e(pdp8_t *pdp8, pdp8_rk8e_callbacks_t *callbacks, pdp8_rk8e_t **rk8e) {
    pdp8_rk8e_t *dev = calloc(1, sizeof(pdp8_rk8e_t));
    if (!dev) {
        return PDP8_ERR_MEMORY;
    }

    int bit = pdp8_alloc_intr_bits(pdp8, 1);
    if (bit < 0) {
        free(dev);
        return bit;
    }

    dev->pdp8 = pdp8;

    dev->device.install = &rk_install;
    dev->device.reset = &rk_reset;
    dev->device.dispatch = &rk_dispatch;
    dev->device.free = &rk_free;

    dev->intr_bit = 1 << bit++;

    dev->callbacks = *callbacks;
    
    int ret = pdp8_install_device(pdp8, &dev->device);
    if (ret < 0) {
        free(dev);
        return ret;
    }

    *rk8e = dev;
    return 0;
}

static int rk_install(pdp8_device_t *dev, pdp8_t *pdp8) {
    if (pdp8->device_handlers[RK_DEVICE_ID] != NULL) {
        return PDP8_ERR_CONFLICT;
    }

    pdp8->device_handlers[RK_DEVICE_ID] = dev;
    return 0;
}

static void rk_reset(pdp8_device_t *dev) {
    pdp8_rk8e_t *rk = (pdp8_rk8e_t *)dev;
}

static void rk_free(pdp8_device_t *dev) {        
    free(dev);
}

static void rk_dispatch(pdp8_device_t *dev, pdp8_t *pdp8, uint12_t opword) {
    
}


