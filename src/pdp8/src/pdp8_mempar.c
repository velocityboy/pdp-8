/**
 * IOT dispatch for MP8-E: Memory Parity
 */
#include <stdlib.h>
 
#include "pdp8/devices.h"
 
static int mp_install(pdp8_device_t *dev, pdp8_t *pdp8);
static void mp_reset(pdp8_device_t *dev);
static void mp_dispatch(pdp8_device_t *dev, pdp8_t *pdp8, uint12_t opword);
static void mp_free(pdp8_device_t *dev);
 
int pdp8_install_mempar(pdp8_t *pdp8) {
    /* for now, we have no extra device state to track */
    pdp8_device_t *dev = calloc(1, sizeof(pdp8_device_t));
    if (!dev) {
        return PDP8_ERR_MEMORY;
    }
 
    dev->install = &mp_install;
    dev->reset = &mp_reset;
    dev->dispatch = &mp_dispatch;
    dev->free = &mp_free;
 
    int ret = pdp8_install_device(pdp8, dev);
    if (ret < 0) {
        free(dev);
        return ret;
    }

    return 0;
}

int mp_install(pdp8_device_t *dev, pdp8_t *pdp8) {
    if (pdp8->device_handlers[010] != NULL) {
        return PDP8_ERR_CONFLICT;
    }

    pdp8->device_handlers[010] = dev;

    return 0;
}

void mp_reset(pdp8_device_t *dev) {

}

void mp_free(pdp8_device_t *dev) {
    
}

void mp_dispatch(pdp8_device_t *dev, pdp8_t *pdp8, uint12_t opword) {
    /* implementation that assumes perfect memory, because some software assumes the facility
     * exists without doing an SPO (6107)
     */
    switch (opword) {
        case DPI:
        case EPI:
        case CMP:
        case CEP:   /* technically this should cause a parity error in the next instruction */
            break;

        case SMP:
        case SMP_CMP:
        case SPO:
            pdp8->pc = INC12(pdp8->pc);
            break;
    }
}


