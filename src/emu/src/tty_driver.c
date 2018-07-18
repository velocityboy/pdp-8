#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/termios.h>
#include <unistd.h>

#include "pdp8/devices.h"

static void tty_free(void *);
static void tty_kbd_ready(void *ctx);
static void tty_print(void *ctx, char ch);
static void tty_service(void *ctx);

static const int SERVICE_INTERVAL = 256;
static const int INPUT_BUFFER_SIZE = 256;
static uint8_t end_key[] = { 033, 0117, 0120, 0 };  /* encoding of F1 */

typedef struct tty_driver_t {
    pdp8_t *pdp8;
    pdp8_console_t *console;
    uint8_t input_buffer[INPUT_BUFFER_SIZE];
    int enq_ptr;
    int deq_ptr;
    int started;
    struct termios def_termios;
    int hw_busy;
} tty_driver_t;

static void try_send(tty_driver_t *tty);    

tty_driver_t *emu_install_tty(pdp8_t *pdp8) {
    tty_driver_t *tty = calloc(1, sizeof(tty_driver_t));

    tty->pdp8 = pdp8;

    pdp8_console_callbacks_t callbacks;

    callbacks.ctx = tty;
    callbacks.free = &tty_free;
    callbacks.kbd_ready = &tty_kbd_ready;
    callbacks.print = &tty_print;
        
    int ret = pdp8_install_console(pdp8, &callbacks, &tty->console);
    if (ret < 0) {
        free(tty);
        return NULL;
    }

    pdp8_schedule(pdp8, SERVICE_INTERVAL, &tty_service, tty);
    return tty;
}

void emu_start_tty(tty_driver_t *tty) {
    if (tty->started) {
        return;
    }
    tcgetattr(0, &tty->def_termios);
    struct termios t = tty->def_termios;

    t.c_lflag &= ~(ICANON|ISIG|IXON|IXOFF|IEXTEN);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);

    tty->started = 1;
}

void emu_end_tty(tty_driver_t *tty) {
    if (!tty->started) {
        return;
    }

    tcsetattr(0, TCSANOW, &tty->def_termios);
    tty->started = 0;
}

static void tty_free(void *ctx) {    
    tty_driver_t *tty = ctx;
    emu_end_tty(tty);
}

static void tty_kbd_ready(void *ctx) {
    tty_driver_t *tty = ctx;
    tty->hw_busy = 0;
    try_send(tty);
}

static void tty_print(void *ctx, char ch) {
    /* characters come in with the high bit set */
    putchar(ch & 0177);
    fflush(stdout);
}

static void halt(tty_driver_t *tty) {
    tty->pdp8->halt_reason = PDP8_HALT_FRONT_PANEL;
    tty->pdp8->run = 0;
}

static int enqueue(tty_driver_t *tty, uint8_t ch) {
    int n = (tty->enq_ptr + 1) % INPUT_BUFFER_SIZE;
    if (n == tty->deq_ptr) {
        return 0;
    }

    tty->input_buffer[tty->enq_ptr] = ch;
    tty->enq_ptr = n;

    return 1;
}

static void try_send(tty_driver_t *tty) {
    if (tty->enq_ptr == tty->deq_ptr || tty->hw_busy) {
        return;
    }

    if (pdp8_console_kbd_byte(tty->console, tty->input_buffer[tty->deq_ptr]) == 0) {
        tty->deq_ptr = (tty->deq_ptr + 1) % INPUT_BUFFER_SIZE;
    }
    tty->hw_busy = 1;
}

static void tty_service(void *ctx) {
    tty_driver_t *tty = ctx;
    int end = 0;
    int overflow = 0;

    /* escape sequences come in all at once, so we don't have to preserve
     * state checking for them past calls 
     */    
    while (1) {
        uint8_t ch;

        int n = read(0, &ch, 1);
        if (n < 0) {
            printf("\nfailed reading from stdin -- halting\n");
            halt(tty);
            break;
        }

        if (n == 0) {
            break;
        }
        
        if (ch == end_key[end]) {
            end++;
            if (end_key[end] == 0) {
                halt(tty);
                break;
            }
            continue;
        }

        /* if we get here, we've seen a partial escape sequence but it isn't
         * the 'end' one, so send it along to the hardware.
         */
        for (int i = 0; i < end; i++) {
            if (!enqueue(tty, end_key[i])) {
                overflow++;
            }
        }
        end = 0;

        if (!enqueue(tty, ch)) {
            overflow++;
        }
    }

    if (overflow) {
        putchar('\a');
    }
    
    try_send(tty);
    pdp8_schedule(tty->pdp8, SERVICE_INTERVAL, &tty_service, tty);
}

