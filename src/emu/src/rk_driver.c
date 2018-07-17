#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <unistd.h>

#include "pdp8/devices.h"
#include "rk_driver.h"
#include "emu.h"

static const int SLOTS = 4;

typedef struct rk_driver_t {
    pdp8_t *pdp8;
    pdp8_rk8e_t *rk8e;
    FILE *disks[SLOTS];
    int dev_id;
} rk_driver_t;

static void rk_load_media(void *ctx, int slot, char *name);
static void rk_unload_media(void *ctx, int slot);

static int rk_has_media(void *ctx, int slot);
static int rk_read(void *ctx, int slot, uint32_t offset, uint32_t bytes, uint8_t *buffer);
static int rk_write(void *ctx, int slot, uint32_t offset, uint32_t bytes, uint8_t *buffer);

int emu_install_rk(pdp8_t *pdp8) {
    rk_driver_t *rk = calloc(1, sizeof(rk_driver_t));

    rk->pdp8 = pdp8;

    pdp8_rk8e_callbacks_t callbacks;

    callbacks.ctx = rk;
    callbacks.has_media = &rk_has_media;
    callbacks.read = &rk_read;
    callbacks.write = &rk_write;
        
    int ret = pdp8_install_rk8e(pdp8, &callbacks, &rk->rk8e);
    if (ret < 0) {
        free(rk);
        return ret;
    }

    emu_device_t device;

    memset(&device, 0, sizeof(device));

    device.name = "RK";
    device.description = "RK05 disk system";
    device.slots = SLOTS;
    device.ctx = rk;
    device.load_media = &rk_load_media;
    device.unload_media = &rk_unload_media;
    ret = emu_register_device(&device);
    if (ret < 0) {
        /* TODO need ability to uninstall devices from hardware side */
        return ret;
    }
    rk->dev_id = ret;
    
    return 0;
}

static void rk_load_media(void *ctx, int slot, char *name) {
    if (slot < 0 || slot >= SLOTS) {
        printf("no device named RK%d\n", slot);
        return;
    }

    rk_driver_t *rk = ctx;    
    if (rk->disks[slot]) {
        printf("RK%d: already has media mounted\n", slot);
        return;
    }

    FILE *fp = fopen(name, "r+b");
    if (fp == NULL) {
        printf("RK%d: could not open \"%s\".\n", slot, name);
        return;
    }

    rk->disks[slot] = fp;
    pdp8_rk8e_set_mounted(rk->rk8e, slot, 1);
}

static void rk_unload_media(void *ctx, int slot) {
    if (slot < 0 || slot >= SLOTS) {
        printf("no device named RK%d\n", slot);
        return;
    }

    rk_driver_t *rk = ctx;
    if (!rk->disks[slot]) {
        printf("RK%d: no media mounbted\n", slot);
        return;
    }

    fclose(rk->disks[slot]);
    rk->disks[slot] = NULL;
    pdp8_rk8e_set_mounted(rk->rk8e, slot, 0);
}

static int rk_has_media(void *ctx, int slot) {
    if (slot < 0 || slot >= SLOTS) {
        return 0;
    }
    rk_driver_t *rk = ctx;
    return rk->disks[slot] != NULL;
}

typedef size_t (*fop_t)(void *, size_t, size_t, FILE *);

static int rk_io(void *ctx, int slot, uint32_t offset, uint32_t bytes, uint8_t *buffer, fop_t fop) {
    if (slot < 0 || slot >= SLOTS) {
        return -1;
    }
    rk_driver_t *rk = ctx;
    FILE *fp = rk->disks[slot];

    if (!fp) {
        return -1;
    }

    if (fseek(fp, offset, SEEK_SET) < 0) {
        return -1;
    }

    if (fop(buffer, 1, bytes, fp) != bytes) {
        return -1;
    }

    return 0;
}

static int rk_read(void *ctx, int slot, uint32_t offset, uint32_t bytes, uint8_t *buffer) {
    return rk_io(ctx, slot, offset, bytes, buffer, (fop_t)&fread);
}

static int rk_write(void *ctx, int slot, uint32_t offset, uint32_t bytes, uint8_t *buffer) {
    return rk_io(ctx, slot, offset, bytes, buffer, (fop_t)&fwrite);
}
