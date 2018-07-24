#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdp8_trace.h"
#include "buffer/lin_buffer.h"
#include "buffer/ring_buffer.h"

static const uint32_t INITIAL_TRACE_BUFFER_SIZE = 1024;
static const uint32_t MIN_BUFFER = INITIAL_TRACE_BUFFER_SIZE;
static const uint32_t SIG = 0x54523032;     /* TR02 */

typedef struct trace_reg_values_t {
    uint12_t ac;
    unsigned link : 1;
    unsigned eae_mode_b : 1;
    unsigned gt : 1;
    uint12_t sr;
    uint12_t mq;
    uint12_t sc;
    uint16_t ifr;
    uint16_t dfr;
    uint16_t ibr;
} trace_reg_values_t;

struct pdp8_trace_t {
    pdp8_t *pdp8;
    lin_buffer_t *lin;
    ring_buffer_t *ring;
    uint32_t core_size;
    int out_of_memory;
    int locked;
    trace_reg_values_t initial_regs;
};

#define TRACE_ENABLED(trc) (!((trc)->out_of_memory || (trc)->locked))

typedef enum trace_type_t {
    trc_out_of_memory,
    trc_begin_instruction,
    trc_end_instruction,
    trc_interrupt,
    trc_memory_changed,
} trace_type_t;

typedef enum trace_regs_t {
    /* bitfields */
    treg_link,
    treg_eae_mode_b,
    treg_gt,
    treg_word_first,
    treg_ac = treg_word_first,
    treg_sr,
    treg_mq,
    treg_sc,
    treg_ifr,
    treg_dfr,
    treg_ibr,
    treg_word_last,
} trace_regs_t;

static int alloc_trace_record(pdp8_trace_t *trc, trace_type_t type, int bytes);

static void on_delete(void *ctx, uint8_t type, uint8_t len, rb_ptr_t start);
static void unpack_end_instruction(pdp8_trace_t *trc, int p, trace_reg_values_t *regs);

static inline int put_uint8(pdp8_trace_t *trc, int index, uint8_t data);
static inline int get_uint8(pdp8_trace_t *trc, int index, uint8_t *data);
static inline int put_uint16(pdp8_trace_t *trc, int index, uint16_t data);
static inline int put_uint32(pdp8_trace_t *trc, int index, uint32_t data);
static inline int get_uint16(pdp8_trace_t *trc, int index, uint16_t *data);
static inline int get_uint32(pdp8_trace_t *trc, int index, uint32_t *data);

pdp8_trace_t *pdp8_trace_create(struct pdp8_t *pdp8, uint32_t buffer_size) {
    pdp8_trace_t *trc = calloc(1, sizeof(pdp8_trace_t));
    if (trc == NULL) {
        return NULL;
    }

    if (buffer_size != TRACE_UNLIMITED && buffer_size < MIN_BUFFER) {
        buffer_size = MIN_BUFFER;
    }

    if (buffer_size == TRACE_UNLIMITED) {
        trc->lin = lb_create(INITIAL_TRACE_BUFFER_SIZE);
    } else {
        trc->ring = rb_create(buffer_size, on_delete, trc, RB_OPT_ALLOC_MAKES_SPACE);
    }

    if (trc->lin == NULL && trc->ring == NULL) {
        pdp8_trace_free(trc);
        return NULL;
    }

    trc->pdp8 = pdp8;
    trc->core_size = pdp8->core_size;

    trc->initial_regs.ac = pdp8->ac;
    trc->initial_regs.link = pdp8->link;
    trc->initial_regs.eae_mode_b = pdp8->eae_mode_b;
    trc->initial_regs.gt = pdp8->gt;
    trc->initial_regs.sr = pdp8->sr;
    trc->initial_regs.mq = pdp8->mq;
    trc->initial_regs.sc = pdp8->sc;
    trc->initial_regs.ifr = pdp8->ifr;
    trc->initial_regs.dfr = pdp8->dfr;
    trc->initial_regs.ibr = pdp8->ibr;

    return trc;
}

void pdp8_trace_free(pdp8_trace_t *trc) {
    if (trc) {
        if (trc->lin) {
            lb_destroy(trc->lin);
        }
        if (trc->ring) {
            rb_destroy(trc->ring);
        }
        free(trc);
    }
}

void pdp8_trace_begin_instruction(pdp8_trace_t *trc) {
    if (!TRACE_ENABLED(trc)) {
        return;
    }

    pdp8_t *pdp8 = trc->pdp8;

    /* TODO only a handful of instructions (in the EAE) are two words.
     * special case them to reduce the size of the trace.
     */
    int rec = alloc_trace_record(trc, trc_begin_instruction, 3 * sizeof(uint16_t));

    if (rec >= 0) {
        rec = put_uint16(trc, rec, pdp8->pc);
        uint12_t addr = pdp8->pc;
        rec = put_uint16(trc, rec, pdp8->core[pdp8->ifr | addr]);
        addr = INC12(addr);
        rec = put_uint16(trc, rec, pdp8->core[pdp8->ifr | addr]);
    }
}

void pdp8_trace_end_instruction(pdp8_trace_t *trc) {
    if (!TRACE_ENABLED(trc)) {
        return;
    }

    pdp8_t *pdp8 = trc->pdp8;

    uint16_t bitregs = 0;
    if (pdp8->link) bitregs |= (1 << treg_link);
    if (pdp8->eae_mode_b) bitregs |= (1 << treg_eae_mode_b);
    if (pdp8->gt) bitregs |= (1 << treg_gt);

    int rec = alloc_trace_record(trc, trc_end_instruction, 8 * sizeof(uint16_t));

    if (rec >= 0) {
        rec = put_uint16(trc, rec, bitregs);        /* 1 */
        rec = put_uint16(trc, rec, pdp8->ac);       /* 2 */
        rec = put_uint16(trc, rec, pdp8->sr);       /* 3 */
        rec = put_uint16(trc, rec, pdp8->mq);       /* 4 */
        rec = put_uint16(trc, rec, pdp8->sc);       /* 5 */
        rec = put_uint16(trc, rec, pdp8->ifr);      /* 6 */
        rec = put_uint16(trc, rec, pdp8->dfr);      /* 7 */
        rec = put_uint16(trc, rec, pdp8->ibr);      /* 8 */
    }
}

void pdp8_trace_interrupt(pdp8_trace_t *trc) {
    if (!TRACE_ENABLED(trc)) {
        return;
    }

    alloc_trace_record(trc, trc_interrupt, 0);
}

void pdp8_trace_memory_write(pdp8_trace_t *trc, uint16_t addr, uint12_t data) {
    if (!TRACE_ENABLED(trc)) {
        return;
    }

    int rec = alloc_trace_record(trc, trc_memory_changed, 2 * sizeof(uint16_t));
    if (rec >= 0) {
        rec = put_uint16(trc, rec, addr);
        rec = put_uint16(trc, rec, data);
    }
}

#define HDR_SIG (0)
#define HDR_IS_RING (1)
#define HDR_CORE (2)
#define HDR_CNT (3)

#define REG_BITS (0)
#define REG_AC (1)
#define REG_SR (2)
#define REG_MQ (3)
#define REG_SC (4)
#define REG_IFR (5)
#define REG_DFR (6)
#define REG_IBR (7)
#define REG_CNT (8)

int pdp8_trace_save(pdp8_trace_t *trc, char *fn) {
    FILE *fp = fopen(fn, "wb");
    if (!fp) {
        return -1;
    }

    uint32_t header[HDR_CNT];
    header[HDR_SIG] = htonl(SIG);
    header[HDR_IS_RING] = htonl(trc->ring != NULL);
    header[HDR_CORE] = htonl(trc->pdp8->core_size);

    if (fwrite(header, sizeof(uint32_t), HDR_CNT, fp) != HDR_CNT) {
        fclose(fp);
        return -1;
    }

    uint16_t regs[REG_CNT];

    uint16_t bitregs = 0;
    if (trc->initial_regs.link) bitregs |= (1 << treg_link);
    if (trc->initial_regs.eae_mode_b) bitregs |= (1 << treg_eae_mode_b);
    if (trc->initial_regs.gt) bitregs |= (1 << treg_gt);

    regs[REG_BITS] = htons(bitregs);
    regs[REG_AC] = htons(trc->initial_regs.ac);
    regs[REG_SR] = htons(trc->initial_regs.sr);
    regs[REG_MQ] = htons(trc->initial_regs.mq);
    regs[REG_SC] = htons(trc->initial_regs.sc);
    regs[REG_IFR] = htons(trc->initial_regs.ifr);
    regs[REG_DFR] = htons(trc->initial_regs.dfr);
    regs[REG_IBR] = htons(trc->initial_regs.ibr);

    if (fwrite(regs, sizeof(uint16_t), REG_CNT, fp) != REG_CNT) {
        fclose(fp);
        return -1;
    }

    int rc = (trc->ring != NULL) ? rb_save(trc->ring, fp) : lb_save(trc->lin, fp);
    if (rc < 0) {
        fclose(fp);
        return -1;
    }

    return fclose(fp) == EOF ? -1 : 0;
}

pdp8_trace_t *pdp8_trace_load(char *fn) {
    FILE *fp = fopen(fn, "rb");
    if (!fp) {
        return NULL;
    }

    uint32_t header[HDR_CNT];
    if (fread(header, sizeof(uint32_t), HDR_CNT, fp) != HDR_CNT) {
        fclose(fp);
        return NULL;
    }

    for (int i = 0; i < HDR_CNT; i++) {
        header[i] = ntohl(header[i]);
    }

    if (header[HDR_SIG] != SIG) {
        fclose(fp);
        return NULL;
    }

    uint16_t regs[REG_CNT];
    if (fread(regs, sizeof(uint16_t), REG_CNT, fp) != REG_CNT) {
        fclose(fp);
        return NULL;
    }

    for (int i = 0; i < REG_CNT; i++) {
        regs[i] = ntohs(regs[i]);
    }

    pdp8_trace_t *trc = calloc(1, sizeof(pdp8_trace_t));
    if (trc == NULL) {
        fclose(fp);
        return trc;
    }

    trc->core_size = header[HDR_CORE];

    if (header[HDR_IS_RING]) {
        trc->ring = rb_load(fp);
    } else {
        trc->lin = lb_load(fp);
    }

    if (trc->ring == NULL && trc->lin == NULL) {
        free(trc);
        fclose(fp);
        return NULL;
    }

    trc->initial_regs.link = (regs[REG_BITS] >> treg_link) & 1;
    trc->initial_regs.eae_mode_b = (regs[REG_BITS] >> treg_eae_mode_b) & 1;
    trc->initial_regs.gt = (regs[REG_BITS] >> treg_gt) & 1;
    trc->initial_regs.ac = regs[REG_AC];
    trc->initial_regs.sr = regs[REG_SR];
    trc->initial_regs.mq = regs[REG_MQ];
    trc->initial_regs.sc = regs[REG_SC];
    trc->initial_regs.ifr = regs[REG_IFR];
    trc->initial_regs.dfr = regs[REG_DFR];
    trc->initial_regs.ibr = regs[REG_IBR];

    /* loaded traces cannot be modified */
    trc->locked = 1;

    return trc;
}

static void list_begin_instruction(pdp8_trace_t *trc, int p, FILE *fp, trace_reg_values_t *reg);
static void list_end_instruction(pdp8_trace_t *trc, int p, FILE *fp, trace_reg_values_t *reg);
static void list_memory_changed(pdp8_trace_t *trc, int p, FILE *fp);

int pdp8_trace_save_listing(pdp8_trace_t *trc, FILE *fp) {
    fprintf(fp, "system has %d words of core\n", (int)trc->core_size);

    trace_reg_values_t regs = trc->initial_regs;

    uint8_t type;
    uint8_t cnt;

    int p = trc->ring ?
        rb_first_event(trc->ring, &type, &cnt) :
        lb_first_event(trc->lin, &type, &cnt);

    int done = 0;
    while (p >= 0 && !done) {
        switch (type) {
            case trc_out_of_memory:
                fprintf(fp, "\ncapture ran out of memory\n");
                done = 1;
                break;

            case trc_begin_instruction:
                list_begin_instruction(trc, p, fp, &regs);
                break;

            case trc_end_instruction:
                list_end_instruction(trc, p, fp, &regs);
                break;

            case trc_interrupt:
                fprintf(fp, "processor interrupt\n");
                break;

            case trc_memory_changed:
                list_memory_changed(trc, p, fp);
                break;

            default:
                fprintf(fp, "unrecognized record in trace capture\n");
                done = 1;
                break;
        }

        p = trc->ring ?
            rb_next_event(trc->ring, p, &type, &cnt) :
            lb_next_event(trc->lin, p, &type, &cnt);
    }

    return 0;
}

static const char *truncated = "\ntrace truncated\n";

static void list_begin_instruction(pdp8_trace_t *trc, int p, FILE *fp, trace_reg_values_t *regs) {
    uint16_t pc;
    p = get_uint16(trc, p, &pc);

    uint16_t ops[2];
    p = get_uint16(trc, p, ops);
    p = get_uint16(trc, p, ops+1);

    char decoded[200];
    pdp8_disassemble(pc, ops, regs->eae_mode_b, decoded, sizeof(decoded));
    fprintf(fp, "%s\n", decoded);
}

static void list_end_instruction(pdp8_trace_t *trc, int p, FILE *fp, trace_reg_values_t *iregs) {
    trace_reg_values_t regs;

    unpack_end_instruction(trc, p, &regs);

    fprintf(fp, "AC %04o%c ", regs.ac, (regs.ac != iregs->ac) ? '*' : ' ');
    fprintf(fp, "SR %04o%c ", regs.sr, (regs.sr != iregs->sr) ? '*' : ' ');
    fprintf(fp, "MQ %04o%c ", regs.mq, (regs.mq != iregs->mq) ? '*' : ' ');
    fprintf(fp, "SC %04o%c ", regs.sc, (regs.sc != iregs->sc) ? '*' : ' ');
    fprintf(fp, "IF %1o%c ", regs.ifr >> 12, (regs.ifr != iregs->ifr) ? '*' : ' ');
    fprintf(fp, "DF %1o%c ", regs.dfr >> 12, (regs.dfr != iregs->dfr) ? '*' : ' ');
    fprintf(fp, "IB %1o%c ", regs.ibr >> 12, (regs.ibr != iregs->ibr) ? '*' : ' ');
    fprintf(fp, "LINK %1o%c ", regs.link, (regs.link != iregs->link) ? '*' : ' ');
    fprintf(fp, "EAEB %1o%c ", regs.eae_mode_b, (regs.eae_mode_b != iregs->eae_mode_b) ? '*' : ' ');
    fprintf(fp, "GT %1o%c ", regs.gt, (regs.gt != iregs->gt) ? '*' : ' ');

    fprintf(fp, "\n");

    *iregs = regs;
}

static void list_memory_changed(pdp8_trace_t *trc, int p, FILE *fp) {
    uint16_t addr;
    uint16_t data;

    p = get_uint16(trc, p, &addr);
    p = get_uint16(trc, p, &data);

    fprintf(fp, "core write [%05o] <= %05o\n", addr, data);
}

static void on_delete(void *ctx, uint8_t type, uint8_t len, rb_ptr_t start) {
    if (type == trc_end_instruction) {
        pdp8_trace_t *trc = ctx;
        unpack_end_instruction(trc, start, &trc->initial_regs);
    }
}

static void unpack_end_instruction(pdp8_trace_t *trc, int p, trace_reg_values_t *regs) {
    uint16_t bitregs;
    p = get_uint16(trc, p, &bitregs);        /* 1 */
    p = get_uint16(trc, p, &regs->ac);       /* 2 */
    p = get_uint16(trc, p, &regs->sr);       /* 3 */
    p = get_uint16(trc, p, &regs->mq);       /* 4 */
    p = get_uint16(trc, p, &regs->sc);       /* 5 */
    p = get_uint16(trc, p, &regs->ifr);      /* 6 */
    p = get_uint16(trc, p, &regs->dfr);      /* 7 */
    p = get_uint16(trc, p, &regs->ibr);      /* 8 */

    regs->link = (bitregs >> treg_link) & 1;
    regs->eae_mode_b = (bitregs >> treg_eae_mode_b) & 1;
    regs->gt = (bitregs >> treg_gt) & 1;
}


static int alloc_trace_record(pdp8_trace_t *trc, trace_type_t type, int bytes) {
    if (trc->ring) {
        return rb_alloc_event(trc->ring, type, bytes);
    }
    return lb_alloc_event(trc->lin, type, bytes);
}

static inline int put_uint8(pdp8_trace_t *trc, int index, uint8_t data) {
    return trc->ring ? rb_put_uint8(trc->ring, index, data) : lb_put_uint8(trc->lin, index, data);
}

static inline int get_uint8(pdp8_trace_t *trc, int index, uint8_t *data) {
    return trc->ring ? rb_get_uint8(trc->ring, index, data) : lb_get_uint8(trc->lin, index, data);
}

static inline int put_uint16(pdp8_trace_t *trc, int index, uint16_t data) {
    return trc->ring ? rb_put_uint16(trc->ring, index, data) : lb_put_uint16(trc->lin, index, data);
}

static inline int put_uint32(pdp8_trace_t *trc, int index, uint32_t data) {
    return trc->ring ? rb_put_uint32(trc->ring, index, data) : lb_put_uint32(trc->lin, index, data);
}

static inline int get_uint16(pdp8_trace_t *trc, int index, uint16_t *data) {
    return trc->ring ? rb_get_uint16(trc->ring, index, data) : lb_get_uint16(trc->lin, index, data);
}

static inline int get_uint32(pdp8_trace_t *trc, int index, uint32_t *data) {
    return trc->ring ? rb_get_uint32(trc->ring, index, data) : lb_get_uint32(trc->lin, index, data);
}
