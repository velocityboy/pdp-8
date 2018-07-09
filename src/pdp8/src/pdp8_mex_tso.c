/**
 * IOT dispatch for KM8-E: Memory Extension and Time Share option
 */
#include <stdlib.h>

#include "pdp8/devices.h"

static int mex_install(pdp8_device_t *dev, pdp8_t *pdp8);
static void mex_reset(pdp8_device_t *dev);
static void mex_dispatch(pdp8_device_t *dev, pdp8_t *pdp8, uint12_t opword);
static void mex_free(pdp8_device_t *dev);

int pdp8_install_mex_tso(pdp8_t *pdp8) {
    /* for now, we have no extra device state to track */
    pdp8_device_t *dev = calloc(1, sizeof(pdp8_device_t));
    if (!dev) {
        return PDP8_ERR_MEMORY;
    }

    dev->install = &mex_install;
    dev->reset = &mex_reset;
    dev->dispatch = &mex_dispatch;
    dev->free = &mex_free;

    int ret = pdp8_install_device(pdp8, dev);
    if (ret < 0) {
        free(dev);
        return ret;
    }

    return 0;
}

static int mex_install(pdp8_device_t *dev, pdp8_t *pdp8) {
    for (int id = 020; id <= 027; id++) {
        if (pdp8->device_handlers[id] != NULL) {
            return PDP8_ERR_CONFLICT;
        }
    }

    for (int id = 020; id <= 027; id++) {
        pdp8->device_handlers[id] = dev;
    }

    return 0;
}

static void mex_reset(pdp8_device_t *dev) {
}

static void mex_free(pdp8_device_t *dev) {
    free(dev);
}


static void mex_dispatch(pdp8_device_t *dev, pdp8_t *pdp8, uint12_t opword) {
    if (MEX_FIELD_OP(opword) == MEX_CDF) {
        pdp8->dfr = MEX_FIELD_OF(opword) << 12;
        return;
    }

    switch (opword) {
        case MEX_RDF:
            pdp8->ac |= (((pdp8->dfr >> 12) & 07) << 3);
            break;
    }
}
