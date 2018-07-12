/**
 * IOT dispatch for KL8-E: TTY/LC8-E: Decwriter interface and PC8-E: reader/punch 
 */
#include <stdlib.h>
 
#include "pdp8/devices.h"



static int tty_install(pdp8_device_t *dev, pdp8_t *pdp8);
static void tty_reset(pdp8_device_t *dev);
static void tty_dispatch(pdp8_device_t *dev, pdp8_t *pdp8, uint12_t opword);
static void tty_free(pdp8_device_t *dev);

static void fire_interrupts(pdp8_console_t *con);
static void prt_interrupt(void *p);

struct pdp8_console_t {
    pdp8_device_t device;
    pdp8_t *pdp8;
    uint32_t kbd_intr_bit;
    uint32_t prt_intr_bit;
    uint32_t rdr_intr_bit;
    uint32_t pun_intr_bit;
    uint32_t all_bits;

    uint32_t dev_flags;
    uint32_t enable_flags;

    uint8_t kbd_buffer;
    uint8_t pun_buffer;

    pdp8_console_callbacks_t callbacks;
};

int pdp8_install_console(pdp8_t *pdp8, pdp8_console_callbacks_t *callbacks, pdp8_console_t **console) {
    pdp8_console_t *dev = calloc(1, sizeof(pdp8_console_t));
    if (!dev) {
        return PDP8_ERR_MEMORY;
    }

    int bit = pdp8_alloc_intr_bits(pdp8, 4);
    if (bit < 0) {
        free(dev);
        return bit;
    }

    dev->pdp8 = pdp8;

    dev->device.install = &tty_install;
    dev->device.reset = &tty_reset;
    dev->device.dispatch = &tty_dispatch;
    dev->device.free = &tty_free;

    dev->kbd_intr_bit = 1 << bit++;
    dev->prt_intr_bit = 1 << bit++;
    dev->rdr_intr_bit = 1 << bit++;
    dev->pun_intr_bit = 1 << bit++;
    dev->all_bits = 
        dev->kbd_intr_bit |
        dev->prt_intr_bit |
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

    *console = dev;
    return 0;
}

static int tty_install(pdp8_device_t *dev, pdp8_t *pdp8) {
    for (int id = 001; id <= 004; id++) {
        pdp8->device_handlers[id] = dev;
    }

    return 0;
}


int pdp8_console_kbd_byte(pdp8_console_t *con, uint8_t ch) {
    if (con->dev_flags & con->kbd_intr_bit) {
        return PDP8_ERR_BUSY;
    }

    /* NB TTY and original DECwriter set the high bit on input data */
    con->kbd_buffer = ch | 0200;

    con->dev_flags |= con->kbd_intr_bit;
    fire_interrupts(con);

    return 0;
}

int pdp8_console_rdr_byte(pdp8_console_t *dev, uint8_t ch) {
    return 0;
}

static void tty_reset(pdp8_device_t *dev) {
    pdp8_console_t *con = (pdp8_console_t *)dev;
    con->dev_flags = 0;
    con->enable_flags = con->all_bits;
    fire_interrupts(con);
}

static void tty_free(pdp8_device_t *dev) {
    free(dev);
}

static void tty_dispatch(pdp8_device_t *dev, pdp8_t *pdp8, uint12_t opword) {
    pdp8_console_t *con = (pdp8_console_t *)dev;
    switch (opword) {
        case KCF:
            con->dev_flags &= ~con->kbd_intr_bit;
            fire_interrupts(con);
            pdp8_schedule(pdp8, 20, con->callbacks.kbd_ready, con->callbacks.ctx);
            break;

        case KSF: 
            if (con->dev_flags & con->kbd_intr_bit) {
                pdp8->pc = INC12(pdp8->pc);
            }
            break;

        case KCC:
            con->dev_flags &= ~con->kbd_intr_bit;
            fire_interrupts(con);
            con->pdp8->ac = 0;
            pdp8_schedule(pdp8, 20, con->callbacks.kbd_ready, con->callbacks.ctx);
            break;
        
        case KRS:
            con->pdp8->ac |= con->kbd_buffer;
            break;

        case KIE:
            /* NB KIE affects the interrupt enables for the whole device, not just the kbd */
            if (con->pdp8->ac & BIT11) {
                con->enable_flags |= (con->kbd_intr_bit | con->prt_intr_bit);
            } else {
                con->enable_flags &= ~(con->kbd_intr_bit | con->prt_intr_bit);
            }
            fire_interrupts(con);
            break;

        case KRB:
            con->dev_flags &= ~con->kbd_intr_bit;
            fire_interrupts(con);
            con->pdp8->ac = con->kbd_buffer;
            pdp8_schedule(pdp8, 20, con->callbacks.kbd_ready, con->callbacks.ctx);
            break;

        case TFL:
            con->dev_flags |= con->prt_intr_bit;
            fire_interrupts(con);
            break;

        case TSF:
            if (con->dev_flags & con->prt_intr_bit) {
                pdp8->pc = INC12(pdp8->pc);
            }
            break;

        case TCF:
            con->dev_flags &= ~con->prt_intr_bit;
            fire_interrupts(con);
            break;

        case TPC:
            con->callbacks.print(con->callbacks.ctx, con->pdp8->ac & 0177);
            pdp8_schedule(pdp8, 20, prt_interrupt, con);
            break;

        case TSK:
            if (con->pdp8->intr_mask & (con->prt_intr_bit | con->kbd_intr_bit)) {
                pdp8->pc = INC12(pdp8->pc);
            }
            return;
        
        case TLS:
            /* TODO this is TCF + TPC, but we immediately turn around and set
             * the done flag again. we should introduce some real-world
             * delay here.
             */
            con->dev_flags &= ~con->prt_intr_bit;
            fire_interrupts(con);
            con->callbacks.print(con->callbacks.ctx, con->pdp8->ac & 0177);
            pdp8_schedule(pdp8, 20, prt_interrupt, con);
            break;
    }
}

void fire_interrupts(pdp8_console_t *con) {
    /* set interrupt bits if device flag is set and enable is on */
    uint32_t mask = con->dev_flags & con->enable_flags;
    con->pdp8->intr_mask = (con->pdp8->intr_mask & ~con->all_bits) | mask;
}

void prt_interrupt(void *p) {
    pdp8_console_t *con = p;
    con->dev_flags |= con->prt_intr_bit;
    fire_interrupts(con);
}
