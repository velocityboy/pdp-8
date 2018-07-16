#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <unistd.h>

#include "pdp8/devices.h"
#include "emu.h"

typedef struct pt_driver_t {
    pdp8_t *pdp8;
    pdp8_punch_t *punch;
    FILE *frdr;
    FILE *fpun;
    int rdr_dev_id;
    int pun_dev_id;
    int halt_on_eof;
} pt_driver_t;

static void ptr_load_media(void *ctx, int slot, char *name);
static void ptr_unload_media(void *ctx, int slot);
static void ptp_load_media(void *ctx, int slot, char *name);
static void ptp_unload_media(void *ctx, int slot);
static char **ptr_get_setting_names(void *ctx);
static int  ptr_get_setting(void *ctx, char *name, int *val);
static int  ptr_set_setting(void *ctx, char *name, int val);

static int emu_load_reader(pt_driver_t *pt, FILE *fp);
static int emu_load_punch(pt_driver_t *pt, FILE *fp);
        
static void pt_free(void *);
static void pt_rdr_ready(void *ctx);
static void pt_punch(void *ctx, uint8_t ch);
static void pt_service(void *ctx);


static void try_send(pt_driver_t *pt);    

int emu_install_pt(pdp8_t *pdp8) {
    pt_driver_t *pt = calloc(1, sizeof(pt_driver_t));

    pt->pdp8 = pdp8;

    pdp8_punch_callbacks_t callbacks;

    callbacks.ctx = pt;
    callbacks.free = &pt_free;
    callbacks.rdr_ready = &pt_rdr_ready;
    callbacks.punch = &pt_punch;
        
    int ret = pdp8_install_punch(pdp8, &callbacks, &pt->punch);
    if (ret < 0) {
        free(pt);
        return ret;
    }

    emu_device_t device;

    memset(&device, 0, sizeof(device));

    device.name = "PTR";
    device.description = "paper tape reader";
    device.slots = 1;
    device.ctx = pt;
    device.load_media = &ptr_load_media;
    device.unload_media = &ptr_unload_media;
    device.get_setting_names = &ptr_get_setting_names;
    device.get_setting = &ptr_get_setting;
    device.set_setting = &ptr_set_setting;
    ret = emu_register_device(&device);
    if (ret < 0) {
        /* TODO need ability to uninstall devices from hardware side */
        return ret;
    }
    pt->rdr_dev_id = ret;

    memset(&device, 0, sizeof(device));
    
    device.name = "PTP";
    device.description = "paper tape punch";
    device.slots = 1;
    device.ctx = pt;
    device.load_media = &ptp_load_media;
    device.unload_media = &ptp_unload_media;
    ret = emu_register_device(&device);
    if (ret < 0) {
        emu_unregister_device(pt->rdr_dev_id);
        /* TODO need ability to uninstall devices from hardware side */
        return ret;
    }
    pt->pun_dev_id = ret;
    
    return 0;
}

static void ptr_load_media(void *ctx, int slot, char *name) {
    pt_driver_t *pt = ctx;
    FILE *fp = fopen(name, "rb");
    if (fp == NULL) {
        printf("PTR0: could not open \"%s\".\n",  name);
        return;
    }

    if (emu_load_reader(pt, fp) < 0) {
        printf("PTR0: device is busy (unload old media first)\n");
        fclose(fp);
    }
}

static void ptr_unload_media(void *ctx, int slot) {
    pt_driver_t *pt = ctx;
    emu_load_reader(pt, NULL);
}

static char **ptr_get_setting_names(void *ctx) {
    static char *names[] = {
        "halt_on_eof",
        NULL,
    };
    return names;
}

static int ptr_get_setting(void *ctx, char *name, int *val) {
    pt_driver_t *pt = ctx;
    if (strcasecmp(name, "halt_on_eof") == 0) {
        *val = pt->halt_on_eof;
        return 0;
    }
    return -1;
}

static int ptr_set_setting(void *ctx, char *name, int val) {
    pt_driver_t *pt = ctx;
    if (strcasecmp(name, "halt_on_eof") == 0) {
        pt->halt_on_eof = val;
        return 0;
    }
    return -1;
}

static void ptp_load_media(void *ctx, int slot, char *name) {
    struct stat st;
    if (stat(name, &st) == 0) {
        printf("PTP0: \"%s\" exists; not overwriting.\n", name);
        return;
    }

    pt_driver_t *pt = ctx;
    FILE *fp = fopen(name, "wb");
    if (fp == NULL) {
        printf("PTP0: could not open \"%s\".\n",  name);
        return;
    }

    if (emu_load_punch(pt, fp) < 0) {
        printf("PTP0: device is busy (unload old media first)\n");
        fclose(fp);
    }
}

static void ptp_unload_media(void *ctx, int slot) {
    pt_driver_t *pt = ctx;
    emu_load_punch(pt, NULL);
}

static int emu_load_reader(pt_driver_t *pt, FILE *fp) {
    if (pt->frdr && fp) {
        return PDP8_ERR_BUSY;
    }
    if (pt->frdr) {
        fclose(pt->frdr);
    }
    pt->frdr = fp;
    try_send(pt);
    return 0;
}

static int emu_load_punch(pt_driver_t *pt, FILE *fp) {
    if (pt->fpun && fp) {
        return PDP8_ERR_BUSY;
    }
    if (pt->fpun) {
        fclose(pt->fpun);
    }
    pt->fpun = fp;
    return 0;
}

static void pt_free(void *ctx) {    
    pt_driver_t *pt = ctx;
    if (pt->frdr) {
        fclose(pt->frdr);
    }
    if (pt->fpun) {
        fclose(pt->fpun);
    }
}

static void pt_rdr_ready(void *ctx) {
    try_send(ctx);
}

static void pt_punch(void *ctx, uint8_t ch) {
    pt_driver_t *pt = ctx;
    if (pt->fpun) {
        fputc(ch, pt->fpun);
    }
}

static void try_send(pt_driver_t *pt) {
    if (pt->frdr == NULL) {
        return;
    }

    int ch = fgetc(pt->frdr);
    if (ch == EOF) {
        if (pt->halt_on_eof) {
            pt->pdp8->run = 0;
            pt->pdp8->halt_reason = PDP8_HALT_DEVICE_REQUEST;
        }
        fclose(pt->frdr);
        pt->frdr = NULL;
        return;
    }

    if (pdp8_punch_rdr_byte(pt->punch, ch) < 0) {
        ungetc(ch, pt->frdr);
    }
}

