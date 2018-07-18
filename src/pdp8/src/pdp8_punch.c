/**
 * IOT dispatch for PC8-E: reader/punch (and the older 750C/75E) 
 */
 #include <stdlib.h>
 
#include "pdp8/devices.h"


static int pun_install(pdp8_device_t *dev, pdp8_t *pdp8);
static void pun_reset(pdp8_device_t *dev);
static void pun_dispatch(pdp8_device_t *dev, pdp8_t *pdp8, uint12_t opword);
static void pun_free(pdp8_device_t *dev);

static void fire_interrupts(pdp8_punch_t *con);
static void pun_interrupt(void *p);

struct pdp8_punch_t {
    pdp8_device_t device;
    pdp8_t *pdp8;
    uint32_t rdr_intr_bit;
    uint32_t pun_intr_bit;
    uint32_t all_bits;

    uint32_t dev_flags;
    uint32_t enable_flags;

    uint8_t rdr_buffer;

    pdp8_punch_callbacks_t callbacks;
};

int pdp8_install_punch(pdp8_t *pdp8, pdp8_punch_callbacks_t *callbacks, pdp8_punch_t **punch) {
    pdp8_punch_t *dev = calloc(1, sizeof(pdp8_punch_t));
    if (!dev) {
        return PDP8_ERR_MEMORY;
    }

    int bit = pdp8_alloc_intr_bits(pdp8, 2);
    if (bit < 0) {
        free(dev);
        return bit;
    }

    dev->pdp8 = pdp8;

    dev->device.install = &pun_install;
    dev->device.reset = &pun_reset;
    dev->device.dispatch = &pun_dispatch;
    dev->device.free = &pun_free;

    dev->rdr_intr_bit = 1 << bit++;
    dev->pun_intr_bit = 1 << bit++;
    dev->all_bits = 
        dev->rdr_intr_bit |
        dev->pun_intr_bit;

    dev->dev_flags = 0;
    dev->enable_flags = dev->all_bits;

    dev->callbacks = *callbacks;
    
    int ret = pdp8_install_device(pdp8, &dev->device);
    if (ret < 0) {
        free(dev);
        return ret;
    }

    *punch = dev;
    return 0;
}

static int pun_install(pdp8_device_t *dev, pdp8_t *pdp8) {
    for (int id = 001; id <= 002; id++) {
        if (pdp8->device_handlers[id] != NULL) {
            return PDP8_ERR_CONFLICT;
        }
    }

    for (int id = 001; id <= 002; id++) {        
        pdp8->device_handlers[id] = dev;
    }

    return 0;
}

int pdp8_punch_rdr_byte(pdp8_punch_t *pun, uint8_t ch) {
    if (pun->dev_flags & pun->rdr_intr_bit) {
        return PDP8_ERR_BUSY;
    }

    pun->rdr_buffer = ch;
    pun->dev_flags |= pun->rdr_intr_bit;
    fire_interrupts(pun);

    return 0;
}

static void pun_reset(pdp8_device_t *dev) {
    pdp8_punch_t *pun = (pdp8_punch_t *)dev;
    pun->dev_flags = 0;
    pun->enable_flags = pun->all_bits;
    fire_interrupts(pun);
}

static void pun_free(pdp8_device_t *dev) {
    free(dev);
}

static void pun_dispatch(pdp8_device_t *dev, pdp8_t *pdp8, uint12_t opword) {
    pdp8_punch_t *pun = (pdp8_punch_t *)dev;
    switch (opword) {
        case RPE:
            pun->enable_flags |= (pun->rdr_intr_bit | pun->pun_intr_bit);
            fire_interrupts(pun);
            break;

        case RSF:
            if (pun->dev_flags & pun->rdr_intr_bit) {
                pdp8->pc = INC12(pdp8->pc);
            }
            break;

        case RRB:
            pun->pdp8->ac |= pun->rdr_buffer;
            pun->dev_flags &= ~pun->rdr_intr_bit;
            fire_interrupts(pun);            
            break;

        case RFC:
            pun->dev_flags &= ~pun->rdr_intr_bit;
            fire_interrupts(pun);
            pdp8_schedule(pdp8, 20, pun->callbacks.rdr_ready, pun->callbacks.ctx);
            break;

        case RRB_RFC:
            pun->pdp8->ac |= pun->rdr_buffer;
            pun->dev_flags &= ~pun->rdr_intr_bit;
            fire_interrupts(pun);
            pdp8_schedule(pdp8, 20, pun->callbacks.rdr_ready, pun->callbacks.ctx);
            break;
        
        case PCE:
            pun->enable_flags &= ~(pun->rdr_intr_bit | pun->pun_intr_bit);
            fire_interrupts(pun);
            break;

        case PSF:
            if (pun->dev_flags & pun->pun_intr_bit) {
                pdp8->pc = INC12(pdp8->pc);
            }
            break;

        case PCF:
            pun->dev_flags &= ~pun->pun_intr_bit;
            fire_interrupts(pun);
            break;

        case PPC:
            pun->callbacks.punch(pun->callbacks.ctx, pun->pdp8->ac & 0377);
            pdp8_schedule(pdp8, 20, pun_interrupt, pun);
            break;

        
        case PLS:
            pun->dev_flags &= ~pun->pun_intr_bit;
            fire_interrupts(pun);
            pun->callbacks.punch(pun->callbacks.ctx, pun->pdp8->ac & 0377);
            pdp8_schedule(pdp8, 20, pun_interrupt, pun);
            break;
    }
}

void fire_interrupts(pdp8_punch_t *pun) {
    /* set interrupt bits if device flag is set and enable is on */
    uint32_t mask = pun->dev_flags & pun->enable_flags;
    pun->pdp8->intr_mask = (pun->pdp8->intr_mask & ~pun->all_bits) | mask;
}

void pun_interrupt(void *p) {
    pdp8_punch_t *pun = p;
    pun->dev_flags |= pun->pun_intr_bit;
    fire_interrupts(pun);
}
