/**
 * IOT dispatch for RK8E: controller for RK05
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
#include "pdp8/devices.h"
#include "pdp8/logger.h"

/* status register - see M7104, D02A data buf & status (6RK5) */
static const uint12_t DRST_DONE             = 04000;
static const uint12_t DRST_RDY_SRW          = 02000;
static const uint12_t DRST_SEEK_ERR         = 00400;
static const uint12_t DRST_DISK_NOT_READY   = 00200;
static const uint12_t DRST_BUSY_ERR         = 00100;
static const uint12_t DRST_TIMEOUT_ERR      = 00040;
static const uint12_t DRST_WRITE_LOCK_ERR   = 00020;
static const uint12_t DRST_CRC_ERR          = 00010;
static const uint12_t DRST_DATA_LATE_ERR    = 00004;
static const uint12_t DRST_STATUS_ERR       = 00002;
static const uint12_t DRST_CYL_ADDR_ERR     = 00001;
static const uint12_t DRST_ERROR_BITS = 
    DRST_BUSY_ERR | DRST_TIMEOUT_ERR | DRST_WRITE_LOCK_ERR | DRST_CRC_ERR |
    DRST_DATA_LATE_ERR | DRST_STATUS_ERR | DRST_CYL_ADDR_ERR;

/* command register - see M7105, D05 major register */
static const uint12_t DLDC_EXT_CYL_ADDR     = 00001;            /* hi bit of cylinder address in DA register */
static const uint12_t DLDC_UNIT_SEL         = 00006;            /* unit number */
static const int DLDC_UNIT_SEL_SHIFT = 1;
static const uint12_t DLDC_EMA              = 00070;            /* extended memory addr (data field) */
static const int DLDC_EMA_SHIFT = 3;
static const uint12_t DLDC_HALF_BLOCK       = 00100;            /* only transfer half a block */
static const uint12_t DLDC_SEEK_DONE        = 00200;            /* set done flag at end of seek */
static const uint12_t DLDC_ENA_INT          = 00400;            /* enable interrupts */
static const uint12_t DLDC_FUNCTION         = 07000;            /* function to perform */
static const int DLDC_FUNCTION_SHIFT = 9;

static const uint12_t DLAG_SECTOR           = 00017;
static const uint12_t DLAG_SURFACE          = 00020;
static const uint12_t DLAG_CYLINDER         = 07740;
static const int DLAG_CYLINDER_SHIFT        = 5;

typedef enum rk8e_drive_command_t {
    drf_read = 0,
    drf_read_all = 1,
    drf_write_lock = 2,
    drf_seek = 3,
    drf_write = 4,
    drf_write_all = 5,
} rk8e_drive_command_t;

static const int RK_DEVICE_ID = 074;

/* drive geometry */
static const int DRIVES = 4;
static const int CYLINDERS = 203; 
static const int WORDS_PER_SECTOR = 256;

static int rk_install(pdp8_device_t *dev, pdp8_t *pdp8);
static void rk_reset(pdp8_device_t *dev);
static void rk_dispatch(pdp8_device_t *dev, pdp8_t *pdp8, uint12_t opword);
static void rk_free(pdp8_device_t *dev);

static void drive_start(pdp8_rk8e_t *rk, rk8e_drive_command_t cmd, int cyl);
static void drive_schedule(void *drive);

static void fire_interrupt(pdp8_rk8e_t *rk);

typedef struct pdp8_rk05_t {
    struct pdp8_rk8e_t *rk;
    int mounted;
    int busy;
    int cyl;
    unsigned flags;
    rk8e_drive_command_t func;
} pdp8_rk05_t;

static const unsigned RK05_PANEL_RO = 00001;
static const unsigned RK05_WLOCK_RO = 00002;
static const unsigned RK05_READ_ONLY = RK05_PANEL_RO | RK05_WLOCK_RO;

struct pdp8_rk8e_t {
    pdp8_device_t device;
    pdp8_t *pdp8;

    uint12_t status;
    uint12_t command;
    uint12_t memaddr;
    uint12_t dskaddr;

    int ctrlr_busy;

    uint32_t intr_bit;
    int logid;
    
    pdp8_rk8e_callbacks_t callbacks;

    pdp8_rk05_t drives[DRIVES];
};

/* timings based on a 1.2 usec instruction time */
static const int SETTLE_TIME = 30833;       /* 37 ms */
static const int TRACK_SEEK_TIME = 1666;    /* 2 ms */
static const int AVG_ROT_LATENCY = 16666;   /* 20 ms */

static int seek_operation_time(int fromcyl, int tocyl) {
    if (fromcyl != tocyl) {
        /* we only pay settle time if the heads move */
        return SETTLE_TIME + TRACK_SEEK_TIME * abs(tocyl - fromcyl);
    }

    return 0;
}

static int io_operation_time(int fromcyl, int tocyl) {
    return seek_operation_time(fromcyl, tocyl) + AVG_ROT_LATENCY;
}

static inline pdp8_rk05_t *selected_unit(struct pdp8_rk8e_t *rk) {
    return rk->drives + ((rk->command & DLDC_UNIT_SEL) >> DLDC_UNIT_SEL_SHIFT);
}

static inline rk8e_drive_command_t selected_function(struct pdp8_rk8e_t *rk) {
    return (rk->command & DLDC_FUNCTION) >> DLDC_FUNCTION_SHIFT;
}

static inline int selected_cylinder(struct pdp8_rk8e_t *rk) {
    /* the cylinder won't all fit in the disk address register so the
     * high bit is stored in the command register 
     */
    int cyl = (rk->dskaddr & DLAG_CYLINDER) >> DLAG_CYLINDER_SHIFT;
    if (rk->command & DLDC_EXT_CYL_ADDR) {
        cyl |= 0200;
    }
    return cyl;
}

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

    dev->logid = logger_add_category("RK");

    for (int i = 0; i < DRIVES; i++) {
        dev->drives[i].rk = dev;
    }
    
    int ret = pdp8_install_device(pdp8, &dev->device);
    if (ret < 0) {
        free(dev);
        return ret;
    }

    *rk8e = dev;
    return 0;
}

void pdp8_rk8e_set_mounted(pdp8_rk8e_t *rk, int slot, int mounted) {
    rk->drives[slot].mounted = mounted;
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
    pdp8_rk8e_t *rk = (pdp8_rk8e_t *)dev;

    switch (opword) {
        case DSKP:
            /* skip on done or error - identical condition to interrupt conditions */
            if (rk->status & (DRST_ERROR_BITS | DRST_DONE)) {
                pdp8->pc = INC12(pdp8->pc);
            }
            break;

        case DCLR:
            rk->status = 0;

            /* AC10-11 are decoded to reset the controller, drive, or status register */
            switch (pdp8->ac & 03) {
                case 0: /* status register */
                    logger_log(rk->logid, "DCLR status register");
                    if (rk->ctrlr_busy) {
                        logger_log(rk->logid, "... controller busy");
                        rk->status |= DRST_BUSY_ERR;
                    }
                    break;

                case 1: /* reset controller */
                    logger_log(rk->logid, "DCLR reset controller");
                    rk->command = 0;
                    rk->ctrlr_busy = 0;
                    rk->memaddr = 0;
                    rk->dskaddr = 0;
                    for (int i = 0; i < DRIVES; i++) {
                        pdp8_unschedule(rk->pdp8, &drive_schedule, &rk->drives[i]);
                        rk->drives[i].busy = 0;
                    }
                    break;

                case 2: /* reset drive */
                    logger_log(rk->logid, "DCLR reset drive");
                    /* can't if controller is busy */
                    if (rk->ctrlr_busy) {
                        logger_log(rk->logid, "... controller busy");
                        rk->status |= DRST_BUSY_ERR;
                        break;
                    }
                    /* reset by seeking to 0 */
                    drive_start(rk, drf_seek, 0);
                    break;

                default:
                    logger_log(rk->logid, "DCLR undefined sub-op (AC %04o)", pdp8->ac);
            }
            break;

        case DLAG:
            logger_log(rk->logid, "DLAG dskaddr %4o", pdp8->ac);
            if (rk->ctrlr_busy) {
                logger_log(rk->logid, "... controller busy");
                rk->status |= DRST_BUSY_ERR;
                break;
            }
            rk->dskaddr = pdp8->ac;
            /* oddly, I/O is started by writing this register, not the command register */
            drive_start(rk, selected_function(rk), selected_cylinder(rk));
            break;

        case DLCA:
            logger_log(rk->logid, "DLCA memaddr %4o", pdp8->ac);
            if (rk->ctrlr_busy) {
                logger_log(rk->logid, "... controller busy");
                rk->status |= DRST_BUSY_ERR;
                break;
            }
            /* NB that if extended memory is to be used, the field is stored in 
             * the command register. 
             */
            rk->memaddr = pdp8->ac;
            break;

        case DRST: {
            pdp8_rk05_t *unit = selected_unit(rk);
            rk->status &= ~(DRST_DISK_NOT_READY | DRST_RDY_SRW);

            /* update status with selected drive */
            if (!unit->mounted) {
                rk->status |= DRST_DISK_NOT_READY;
            }
            if (unit->busy) {
                rk->status |= DRST_RDY_SRW;
            }

            logger_log(rk->logid, "DRST status %04o", rk->status);
            pdp8->ac = rk->status;
            break;
        }

        case DLDC:
            logger_log(rk->logid, "DLDC command %04o", pdp8->ac);
            if (rk->ctrlr_busy) {
                logger_log(rk->logid, "... controller busy");
                rk->status |= DRST_BUSY_ERR;
                break;
            }
            rk->command = pdp8->ac;
            rk->status = 0;
            break;
    }
    fire_interrupt(rk);
}

static void drive_start(pdp8_rk8e_t *rk, rk8e_drive_command_t cmd, int cyl) {
    pdp8_rk05_t *drive = selected_unit(rk);
    /* flag errors first */
    if (!drive->mounted) {
        logger_log(rk->logid, "start: selected unit %d is not mounted", drive - rk->drives);
        /* no media, flag not ready */
        rk->status |= DRST_DONE | DRST_DISK_NOT_READY | DRST_STATUS_ERR;
        return;
    }

    /* drive already busy */
    if (drive->busy) {
        logger_log(rk->logid, "start: selected unit %d is already busy", drive - rk->drives);
        rk->status |= DRST_DONE | DRST_STATUS_ERR;
        return;
    }

    /* cylinder out of range */
    if (cyl >= CYLINDERS) {
        logger_log(rk->logid, "start: cylinder %d is out of range", cyl);
        rk->status |= DRST_DONE | DRST_CYL_ADDR_ERR | DRST_STATUS_ERR;
        return;
    }

    switch (cmd) {
        case drf_read:
        case drf_read_all:
        case drf_write:
        case drf_write_all:
            logger_log(rk->logid, "start: scheduling read/write op");
            drive->func = (cmd == drf_read || cmd == drf_read_all) ? drf_read : drf_write;
            drive->busy = 1;
            rk->ctrlr_busy = 1;
            pdp8_schedule(rk->pdp8, io_operation_time(drive->cyl, cyl), &drive_schedule, drive);
            drive->cyl = cyl;
            break;

        case drf_write_lock:
            /* NB the only way to turn this off is device reset */
            logger_log(rk->logid, "start: setting selected unit %d to be write locked", drive - rk->drives);
            drive->flags |= RK05_WLOCK_RO;
            rk->status |= DRST_DONE;
            break;

        case drf_seek:
            drive->func = drf_seek;
            drive->busy = 1;
            /* note that seek does not keep the controller busy, so the system can 
             * service other drives
             */
            logger_log(rk->logid, "start: scheduling seek");
            drive->cyl = cyl;
            pdp8_schedule(rk->pdp8, io_operation_time(drive->cyl, cyl), &drive_schedule, drive);
            break;

        default:
            /* undefined function */
            logger_log(rk->logid, "start: setting error on undefined drive func %d", cmd);
            rk->status |= DRST_DONE | DRST_STATUS_ERR;
            break;
    }
}

static void drive_schedule(void *param) {
    pdp8_rk05_t *drive = param;
    pdp8_rk8e_t *rk = drive->rk;

    logger_log(rk->logid, "schedule: running schedule op for unit %d", drive - rk->drives);

    drive->busy = 0;

    if (drive->mounted == 0) {
        logger_log(rk->logid, "schedule: drive is no longer mounted");
        rk->status |= DRST_STATUS_ERR | DRST_DISK_NOT_READY | DRST_DONE;
        fire_interrupt(rk);
        return;        
    }

    if (drive->func == drf_seek && drive == selected_unit(rk) && (rk->command & DLDC_SEEK_DONE) != 0) {
        logger_log(rk->logid, "schedule: setting done on seek completion");
        rk->status |= DRST_DONE;
        fire_interrupt(rk);
        return;
    }

    if (drive->func == drf_write && (drive->flags & RK05_READ_ONLY) != 0) {
        logger_log(rk->logid, "schedule: setting write lock error");
        rk->status |= DRST_WRITE_LOCK_ERR | DRST_DONE;
        fire_interrupt(rk);
        return;
    }

    assert(drive->func == drf_read || drive->func == drf_write);
    int (*io)(void *ctx, int slot, uint32_t offset, uint32_t bytes, uint8_t *buffer);
    io = (drive->func == drf_read) ? rk->callbacks.read : rk->callbacks.write;
    int slot = drive - rk->drives;

    /*
     * A couple of things make this complicated:
     * - A physical address is 15 bits; a 3 bit field and a 12 bit offset. However,
     *   I/O from the disk does not cross fields; if it would overflow the 12 bit offset,
     *   it wraps to the start of the field.
     * - If we are reading, and half-sector is selected, then we just halve the word count.
     *   However, a half-sector write pads the rest of the sector with zeroes.
     */
    int fileoffs = rk->dskaddr;
    if (rk->command & DLDC_EXT_CYL_ADDR) {
        fileoffs |= 010000;
    }
    fileoffs *= WORDS_PER_SECTOR * sizeof(uint16_t);

    unsigned field = ((rk->command & DLDC_EMA) >> DLDC_EMA_SHIFT) << 12;
    unsigned offset = rk->memaddr;
    unsigned words = (rk->command & DLDC_HALF_BLOCK) ? (WORDS_PER_SECTOR / 2) : WORDS_PER_SECTOR;

    logger_log(rk->logid, "schedule: op is %s physical %5o words %o file offset %o", 
        (drive->func == drf_read) ? "read" : "write", field|offset, words, fileoffs);

    assert(offset < 010000);
    unsigned left_in_field = 010000 - offset;
    unsigned xfer = (words < left_in_field) ? words : left_in_field;

    void *ctx = rk->callbacks.ctx;

    /* TODO gate against reading/writing to nonexistent core */

    int err = io(ctx, slot, fileoffs, xfer * sizeof(uint16_t), (uint8_t*)&rk->pdp8->core[field | offset]) < 0;
    if (err) {
        logger_log(rk->logid, "schedule: initial transfer of %o words failed (%s)", xfer, strerror(errno));
    }
    fileoffs += xfer * sizeof(uint16_t);
    if (!err && xfer < words) {
        xfer = words - xfer;
        err = io(ctx, slot, fileoffs, xfer * sizeof(uint16_t), (uint8_t*)&rk->pdp8->core[field]) < 0;
        fileoffs += xfer * sizeof(uint16_t);
        if (err) {
            logger_log(rk->logid, "schedule: second transfer of %o words failed (%s)", xfer, strerror(errno));
        }
    }

    if (!err && drive->func == drf_write && (rk->command & DLDC_HALF_BLOCK) != 0) {
        uint16_t pad[WORDS_PER_SECTOR / 2];
        memset(pad, 0, sizeof(pad));
        err = io(ctx, slot, fileoffs, sizeof(pad), (uint8_t*)pad) < 0;
        if (err) {
            logger_log(rk->logid, "schedule: zero fill failed (%s)", xfer, strerror(errno));
        }
    }

    if (err) {
        rk->status |= DRST_TIMEOUT_ERR;
    } 

    rk->memaddr = (rk->memaddr + words) & MASK12;
    rk->status |= DRST_DONE;
    rk->ctrlr_busy = 0;

    fire_interrupt(rk);    
}

static void fire_interrupt(pdp8_rk8e_t *rk) {
    uint32_t intr_mask = 0;
    if ((rk->status & (DRST_ERROR_BITS | DRST_DONE)) && (rk->command & DLDC_ENA_INT)) {
        logger_log(rk->logid, "setting interrupt request");
        intr_mask = rk->intr_bit;
    }
    rk->pdp8->intr_mask = (rk->pdp8->intr_mask & ~rk->intr_bit) | intr_mask;
}
