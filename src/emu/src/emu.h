#ifndef _EMU_H_
#define _EMU_H_

typedef struct emu_device_t {
    char *name;
    char *description;
    int slots;
    void *ctx;
    void (*load_media)(void *ctx, int slot, char *name);
    void (*unload_media)(void *ctx, int slot);
} emu_device_t;

extern int emu_register_device(emu_device_t *device);
extern void emu_unregister_device(int dev_id);

#endif
