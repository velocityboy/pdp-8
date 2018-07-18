/**
 * IOT dispatch for KL8-E: TTY/LC8-E: Decwriter interface
 */
#include <stdlib.h>
 
#include "pdp8/devices.h"
#include "pdp8/logger.h"

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
    uint32_t all_bits;

    uint32_t dev_flags;
    uint32_t enable_flags;

    uint8_t kbd_buffer;
    int logid;

    pdp8_console_callbacks_t callbacks;
};

int pdp8_install_console(pdp8_t *pdp8, pdp8_console_callbacks_t *callbacks, pdp8_console_t **console) {
    pdp8_console_t *dev = calloc(1, sizeof(pdp8_console_t));
    if (!dev) {
        return PDP8_ERR_MEMORY;
    }

    int bit = pdp8_alloc_intr_bits(pdp8, 2);
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
    dev->all_bits = 
        dev->kbd_intr_bit |
        dev->prt_intr_bit;

    dev->dev_flags = 0;
    dev->enable_flags = dev->all_bits;

    dev->callbacks = *callbacks;

    dev->logid = logger_add_category("TT");
    
    int ret = pdp8_install_device(pdp8, &dev->device);
    if (ret < 0) {
        free(dev);
        return ret;
    }

    *console = dev;
    return 0;
}

static int tty_install(pdp8_device_t *dev, pdp8_t *pdp8) {
    for (int id = 003; id <= 004; id++) {
        if (pdp8->device_handlers[id] != NULL) {
            return PDP8_ERR_CONFLICT;
        }
    }

    for (int id = 003; id <= 004; id++) {        
        pdp8->device_handlers[id] = dev;
    }

    return 0;
}


int pdp8_console_kbd_byte(pdp8_console_t *con, uint8_t ch) {
    if (con->dev_flags & con->kbd_intr_bit) {
        logger_log(con->logid, "system tried to send kbd %03o but device is busy", ch);
        return PDP8_ERR_BUSY;
    }

    logger_log(con->logid, "system sent kbd %03o but device is busy", ch);

    /* NB TTY and original DECwriter set the high bit on input data */
    con->kbd_buffer = ch | 0200;

    con->dev_flags |= con->kbd_intr_bit;
    fire_interrupts(con);

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
            logger_log(con->logid, "KCF");
            con->dev_flags &= ~con->kbd_intr_bit;
            fire_interrupts(con);
            pdp8_schedule(pdp8, 20, con->callbacks.kbd_ready, con->callbacks.ctx);
            break;

        case KSF: 
            if (con->dev_flags & con->kbd_intr_bit) {
                logger_log(con->logid, "KSF - skipping");
                pdp8->pc = INC12(pdp8->pc);
                return;
            }
            logger_log(con->logid, "KSF - not skipping");
            break;

        case KCC:
            logger_log(con->logid, "KCC");
            con->dev_flags &= ~con->kbd_intr_bit;
            fire_interrupts(con);
            con->pdp8->ac = 0;
            pdp8_schedule(pdp8, 20, con->callbacks.kbd_ready, con->callbacks.ctx);
            break;
        
        case KRS:
            logger_log(con->logid, "KRS - returning %03o", con->kbd_buffer);
            con->pdp8->ac |= con->kbd_buffer;
            break;

        case KIE:
            /* NB KIE affects the interrupt enables for the whole device, not just the kbd */
            if (con->pdp8->ac & BIT11) {
                logger_log(con->logid, "KIE enabling interrupts");
                con->enable_flags |= (con->kbd_intr_bit | con->prt_intr_bit);
            } else {
                logger_log(con->logid, "KIE disabling interrupts");
                con->enable_flags &= ~(con->kbd_intr_bit | con->prt_intr_bit);
            }
            fire_interrupts(con);
            break;

        case KRB:
            logger_log(con->logid, "KRB returning %03o", con->kbd_buffer);
            con->dev_flags &= ~con->kbd_intr_bit;
            fire_interrupts(con);
            con->pdp8->ac = con->kbd_buffer;
            pdp8_schedule(pdp8, 20, con->callbacks.kbd_ready, con->callbacks.ctx);
            break;

        case TFL:
            logger_log(con->logid, "TFL");
            con->dev_flags |= con->prt_intr_bit;
            fire_interrupts(con);
            break;

        case TSF:
            if (con->dev_flags & con->prt_intr_bit) {
                logger_log(con->logid, "TSF - skipping");
                pdp8->pc = INC12(pdp8->pc);
                return;
            }
            logger_log(con->logid, "TSF - not skipping");
            break;

        case TCF:
            logger_log(con->logid, "TCF");
            con->dev_flags &= ~con->prt_intr_bit;
            fire_interrupts(con);
            break;

        case TPC:
            logger_log(con->logid, "TPC sending %03o", con->pdp8->ac & 0177);
            con->callbacks.print(con->callbacks.ctx, con->pdp8->ac & 0177);
            pdp8_schedule(pdp8, 20, prt_interrupt, con);
            break;

        case TSK:            
            if (con->pdp8->intr_mask & (con->prt_intr_bit | con->kbd_intr_bit)) {
                logger_log(con->logid, "TSK - skipping");
                pdp8->pc = INC12(pdp8->pc);
                return;
            }
            logger_log(con->logid, "TSK - not skipping");
            return;
        
        case TLS:
            logger_log(con->logid, "TLS sending %03o", con->pdp8->ac & 0177);
            con->dev_flags &= ~con->prt_intr_bit;
            fire_interrupts(con);
            con->callbacks.print(con->callbacks.ctx, con->pdp8->ac & 0177);
            pdp8_schedule(pdp8, 20, prt_interrupt, con);
            break;
    }
}

static void fire_interrupts(pdp8_console_t *con) {
    /* set interrupt bits if device flag is set and enable is on */
    uint32_t mask = con->dev_flags & con->enable_flags;
    con->pdp8->intr_mask = (con->pdp8->intr_mask & ~con->all_bits) | mask;
    logger_log(con->logid, "%s interrupt request", mask ? "asserting" : "releasing");
}

static void prt_interrupt(void *p) {
    pdp8_console_t *con = p;
    logger_log(con->logid, "firing printer interrupt");
    con->dev_flags |= con->prt_intr_bit;
    fire_interrupts(con);
}
