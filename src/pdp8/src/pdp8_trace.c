#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdp8_trace.h"

const uint8_t TRACE_VERSION = 1;
const uint32_t INITIAL_TRACE_BUFFER_SIZE = 1024;

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
    uint8_t *trace;
    uint32_t allocated;
    uint32_t used;
    int out_of_memory;
    int locked;

    /* cached registers to see what changed */
    trace_reg_values_t regs;
};

#define TRACE_ENABLED(trc) (!((trc)->out_of_memory || (trc)->locked))

typedef enum trace_type_t {
    trc_version,
    trc_all_registers,
    trc_out_of_memory,
    trc_system_memory,
    trc_begin_instruction,
    trc_end_instruction,
    trc_interrupt,
    trc_memory_changed,
} trace_type_t;

typedef enum trace_regs_t {
    // bitfields
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

static uint8_t *alloc_trace_record(pdp8_trace_t *trc, int bytes);

static inline uint8_t *put_uint16(uint8_t *p, uint16_t data) {
    *(uint16_t *)p = htons(data);
    return p + sizeof(uint16_t);
}

static inline uint8_t *put_uint32(uint8_t *p, uint32_t data) {
    *(uint32_t *)p = htonl(data);
    return p + sizeof(uint32_t);
}

static inline uint8_t *get_uint16(uint8_t *p, uint16_t *data) {
    *data = ntohs(*(uint16_t *)p);
    return p + sizeof(uint16_t); 
}

static inline uint8_t *get_uint32(uint8_t *p, uint32_t *data) {
    *data = ntohl(*(uint32_t *)p);
    return p + sizeof(uint32_t); 
}

pdp8_trace_t *pdp8_trace_create(struct pdp8_t *pdp8) {
    pdp8_trace_t *trc = calloc(1, sizeof(pdp8_trace_t));
    if (trc == NULL) {
        return NULL;
    }

    trc->trace = malloc(INITIAL_TRACE_BUFFER_SIZE);
    if (trc->trace == NULL) {
        pdp8_trace_free(trc);
        return NULL;
    }

    trc->pdp8 = pdp8;
    trc->allocated = INITIAL_TRACE_BUFFER_SIZE;

    /* must be the first record */
    uint8_t *rec = alloc_trace_record(trc, 2);
    if (rec) {
        *rec++ = trc_version;
        *rec++ = TRACE_VERSION;
    }

    rec = alloc_trace_record(trc, 1 + sizeof(uint32_t));
    if (rec) {
        *rec++ = trc_system_memory;
        rec = put_uint32(rec, pdp8->core_size);
    }

    uint16_t bitfields = 0;
    if (pdp8->link) {
        bitfields |= (1 << treg_link);
    }

    if (pdp8->eae_mode_b) {
        bitfields |= (1 << treg_eae_mode_b);
    }

    if (pdp8->gt) {
        bitfields |= (1 << treg_gt);
    }

    int reg_rec_size = 1 + (treg_word_last - treg_word_first + 1) * sizeof(uint16_t);
    rec = alloc_trace_record(trc, reg_rec_size);
    if (rec) {
        uint8_t *start = rec;
        *rec++ = trc_all_registers;
         
        rec = put_uint16(rec, bitfields);
        rec = put_uint16(rec, pdp8->ac);
        rec = put_uint16(rec, pdp8->sr);
        rec = put_uint16(rec, pdp8->mq);
        rec = put_uint16(rec, pdp8->sc);
        rec = put_uint16(rec, pdp8->ifr);
        rec = put_uint16(rec, pdp8->dfr);
        rec = put_uint16(rec, pdp8->ibr);

        assert((rec - start) == reg_rec_size);
    }

    return trc;
}

void pdp8_trace_free(pdp8_trace_t *trc) {
    if (trc) {
        free(trc->trace);
        free(trc);
    }
}

void pdp8_trace_begin_instruction(pdp8_trace_t *trc) {
    if (!TRACE_ENABLED(trc)) {
        return;
    }

    pdp8_t *pdp8 = trc->pdp8;
    trc->regs.ac = pdp8->ac;
    trc->regs.link = pdp8->link;
    trc->regs.eae_mode_b = pdp8->eae_mode_b;
    trc->regs.gt = pdp8->gt;
    trc->regs.sr = pdp8->sr;
    trc->regs.mq = pdp8->mq;
    trc->regs.sc = pdp8->sc;
    trc->regs.ifr = pdp8->ifr;
    trc->regs.dfr = pdp8->dfr;
    trc->regs.ibr = pdp8->ibr;

    /* TODO only a handful of instructions (in the EAE) are two words.
     * special case them to reduce the size of the trace.
     */
    uint8_t *rec = alloc_trace_record(trc, 1 + 3 * sizeof(uint16_t));

    if (rec) {
        *rec++ = trc_begin_instruction;
        rec = put_uint16(rec, pdp8->pc);
        uint12_t addr = pdp8->pc;
        rec = put_uint16(rec, pdp8->core[pdp8->ifr | addr]);
        addr = INC12(addr);
        rec = put_uint16(rec, pdp8->core[pdp8->ifr | addr]);
    }
}

void pdp8_trace_end_instruction(pdp8_trace_t *trc) {
    if (!TRACE_ENABLED(trc)) {
        return;
    }

    pdp8_t *pdp8 = trc->pdp8;

    /* space for type + uint16 for change field + uint16 bit regs + 
     * ~10 word registers = 25 bytes
     * rounded up in case more reigsters added
     */
    uint8_t buffer[128];
    uint8_t *p;
    uint16_t changed = 0;
    uint16_t bitfields = 0;

    p = buffer;

    *p++ = trc_end_instruction;
    uint16_t *p_changed = (uint16_t *)p;
    p += sizeof(uint16_t);
    uint16_t *p_bitfields = (uint16_t *)p;
    p += sizeof(uint16_t);

#define CHECK_REG16(r) \
    if (pdp8->r != trc->regs.r) { \
        p = put_uint16(p, pdp8-> r); \
        changed |= (1 << treg_##r); \
    }

#define CHECK_BIT(b) \
    if (pdp8->b != trc->regs.b) { \
        bitfields |= (pdp8->b ? 0 : (1 << treg_##b)); \
        changed |= (1 << treg_##b); \
    }   

    CHECK_BIT(link);
    CHECK_BIT(eae_mode_b);
    CHECK_BIT(gt);
    CHECK_REG16(ac);
    CHECK_REG16(sr);
    CHECK_REG16(mq);
    CHECK_REG16(sc);
    CHECK_REG16(ifr);
    CHECK_REG16(dfr);
    CHECK_REG16(ibr);

    assert(p <= buffer + sizeof(buffer));

#undef CHECK_REG16
#undef CHECK_BIT    

    *p_changed = htons(changed);
    *p_bitfields = htons(bitfields);
    
    uint8_t *rec = alloc_trace_record(trc, p - buffer);
    if (rec) {
        memcpy(rec, buffer, p - buffer);
    }
}

void pdp8_trace_interrupt(pdp8_trace_t *trc) {
    if (!TRACE_ENABLED(trc)) {
        return;
    }

    uint8_t *rec = alloc_trace_record(trc, 1);
    if (rec) {
        *rec = trc_interrupt;
    }
}

void pdp8_trace_memory_write(pdp8_trace_t *trc, uint16_t addr, uint12_t data) {
    if (!TRACE_ENABLED(trc)) {
        return;
    }

    uint8_t *rec = alloc_trace_record(trc, 1 + 2 * sizeof(uint16_t));
    if (rec) {
        *rec++ = trc_memory_changed;
        rec = put_uint16(rec, addr);
        rec = put_uint16(rec, data);
    }
}

int pdp8_trace_save(pdp8_trace_t *trc, char *fn) {
    FILE *fp = fopen(fn, "wb");
    if (!fp) {
        return -1;
    }

    size_t wrote = fwrite(trc->trace, 1, trc->used, fp);
    if (wrote != trc->used) {
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

    pdp8_trace_t *trc = calloc(1, sizeof(pdp8_trace_t));
    if (!trc) {
        return NULL;
    }

    fseek(fp, 0L, SEEK_END);
    size_t bytes = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    if ((trc->trace = malloc(bytes)) == NULL) {
        fclose(fp);
        free(trc);
        return NULL;
    }

    if (fread(trc->trace, 1, bytes, fp) != bytes) {
        fclose(fp);
        free(trc->trace);
        free(trc);
        return NULL;
    }

    trc->used = bytes;

    /* loaded traces cannot be appended to */
    trc->locked = 1;

    return trc;
}

static uint8_t *list_all_registers(uint8_t *p, uint8_t *pend, FILE *fp, trace_reg_values_t *regs);
static uint8_t *list_system_memory(uint8_t *p, uint8_t *pend, FILE *fp);
static uint8_t *list_begin_instruction(uint8_t *p, uint8_t *pend, FILE *fp, trace_reg_values_t *regs);
static uint8_t *list_end_instruction(uint8_t *p, uint8_t *pend, FILE *fp, trace_reg_values_t *regs);
static uint8_t *list_memory_changed(uint8_t *p, uint8_t *pend, FILE *fp);

int pdp8_trace_save_listing(pdp8_trace_t *trc, char *fn) {
    FILE *fp = fopen(fn, "wb");
    if (!fp) {
        return -1;
    }

    uint8_t *p = trc->trace;
    uint8_t *pend = trc->trace + trc->used;

    if (pend - p < 2 || *p++ != trc_version) {
        fprintf(fp, "given trace is not valid (no version header)\n");
        fclose(fp);
        return -1;
    }

    uint8_t version = *p++;
    if (version > TRACE_VERSION) {
        fprintf(fp, "given trace is not valid (version stamp is unrecognized)\n");
        fclose(fp);
        return -1;
    }

    int done = 0;

    trace_reg_values_t regs;
    memset(&regs, 0, sizeof(regs));

    while (p < pend && !done) {
        switch (*p++) {
            case trc_out_of_memory:
                fprintf(fp, "\ncapture ran out of memory\n");
                done = 1;
                break;

            case trc_system_memory:
                p = list_system_memory(p, pend, fp);
                break;

            case trc_all_registers:
                p = list_all_registers(p, pend, fp, &regs);
                break;

            case trc_begin_instruction:
                p = list_begin_instruction(p, pend, fp, &regs);
                break;

            case trc_end_instruction:
                p = list_end_instruction(p, pend, fp, &regs);
                break;

            case trc_interrupt:
                fprintf(fp, "processor interrupt\n");
                break;

            case trc_memory_changed:
                p = list_memory_changed(p, pend, fp);
                break;

            default:
                fprintf(fp, "unrecognized record in trace capture\n");
                done = 1;
                break;
        }
    }

    return 0;
}

static const char *truncated = "\ntrace truncated\n";

static uint8_t *list_all_registers(uint8_t *p, uint8_t *pend, FILE *fp, trace_reg_values_t *regs) {
    int reg_rec_size = (treg_word_last - treg_word_first + 1) * sizeof(uint16_t);
    if (pend - p < reg_rec_size) {
        fprintf(fp, "%s", truncated);
        return pend;
    }

    uint8_t *start = p;

    uint16_t bitfields;

    p = get_uint16(p, &bitfields);
    p = get_uint16(p, &regs->ac);
    p = get_uint16(p, &regs->sr);
    p = get_uint16(p, &regs->mq);
    p = get_uint16(p, &regs->sc);
    p = get_uint16(p, &regs->ifr);
    p = get_uint16(p, &regs->dfr);
    p = get_uint16(p, &regs->ibr);

    regs->link = (bitfields >> treg_link) & 01;
    regs->eae_mode_b = (bitfields >> treg_eae_mode_b) & 01;
    regs->gt = (bitfields >> treg_gt) & 01;
    
    assert((p - start) == reg_rec_size);
    return p;
}

static uint8_t *list_system_memory(uint8_t *p, uint8_t *pend, FILE *fp) {
    if (pend - p < sizeof(uint32_t)) {
        fprintf(fp, "%s", truncated);
        return pend;
    }

    uint32_t core = 0;
    p = get_uint32(p, &core);
    fprintf(fp, "system has %d words of core\n", (int)core);

    return p;
}

static uint8_t *list_begin_instruction(uint8_t *p, uint8_t *pend, FILE *fp, trace_reg_values_t *regs) {
    if (pend - p < 3 * sizeof(uint16_t)) {
        fprintf(fp, "%s", truncated);
        return pend;
    }

    uint16_t pc;
    p = get_uint16(p, &pc);

    uint16_t ops[2];
    p = get_uint16(p, ops);
    p = get_uint16(p, ops+1);

    char decoded[200];
    pdp8_disassemble(pc, ops, regs->eae_mode_b, decoded, sizeof(decoded));
    fprintf(fp, "%s\n", decoded);
  
    return p;
}

static uint8_t *list_end_instruction(uint8_t *p, uint8_t *pend, FILE *fp, trace_reg_values_t *regs) {
    int tflag = 0;
    if (pend - p < 2 * sizeof(uint16_t)) {
        tflag = 1;
    }

    uint16_t changed;
    uint16_t bitfields;

    if (!tflag) {
        p = get_uint16(p, &changed);
        p = get_uint16(p, &bitfields);

        int count = 0;

        for (int i = 0; i < 16; i++) {
            if (changed & (1 << i)) {
                count++;
            }
        }
        if (pend - p < count * sizeof(uint16_t)) {
            tflag = 1;
        }
    }

    if (tflag) {
        fprintf(fp, "%s", truncated);
        return pend;
    }

#define RESTORE_BIT(b) \
    if (changed & (1 << treg_##b)) { \
        regs->b = (bitfields >> treg_##b) & 01; \
    }

#define RESTORE_REG16(r) \
    if (changed & (1 << treg_##r)) { \
        p = get_uint16(p, &regs->r); \
    }

    RESTORE_BIT(link);
    RESTORE_BIT(eae_mode_b);
    RESTORE_BIT(gt);

    /* these MUST be in the same order as saved */
    RESTORE_REG16(ac);
    RESTORE_REG16(sr);
    RESTORE_REG16(mq);
    RESTORE_REG16(sc);
    RESTORE_REG16(ifr);
    RESTORE_REG16(dfr);
    RESTORE_REG16(ibr);

#undef RESTORE_BIT
#undef RESTORE_REG16

    fprintf(fp, "AC %04o%c ", regs->ac, (changed & (1 << treg_ac)) ? '*' : ' ');
    fprintf(fp, "SR %04o%c ", regs->sr, (changed & (1 << treg_sr)) ? '*' : ' ');
    fprintf(fp, "MQ %04o%c ", regs->mq, (changed & (1 << treg_mq)) ? '*' : ' ');
    fprintf(fp, "SC %04o%c ", regs->sc, (changed & (1 << treg_sc)) ? '*' : ' ');
    fprintf(fp, "IF %1o%c ", regs->ifr >> 12, (changed & (1 << treg_ifr)) ? '*' : ' ');
    fprintf(fp, "DF %1o%c ", regs->dfr >> 12, (changed & (1 << treg_dfr)) ? '*' : ' ');
    fprintf(fp, "IB %1o%c ", regs->ibr >> 12, (changed & (1 << treg_ibr)) ? '*' : ' ');
    fprintf(fp, "LINK %1o%c ", regs->link >> 12, (changed & (1 << treg_link)) ? '*' : ' ');
    fprintf(fp, "EAEB %1o%c ", regs->eae_mode_b >> 12, (changed & (1 << treg_eae_mode_b)) ? '*' : ' ');
    fprintf(fp, "GT %1o%c ", regs->gt >> 12, (changed & (1 << treg_gt)) ? '*' : ' ');
    
    
    fprintf(fp, "\n");

    return p;
}

static uint8_t *list_memory_changed(uint8_t *p, uint8_t *pend, FILE *fp) {
    if (pend - p < 2 * sizeof(uint16_t)) {
        fprintf(fp, "%s", truncated);
        return pend;
    }    

    uint16_t addr;
    uint16_t data;

    p = get_uint16(p, &addr);
    p = get_uint16(p, &data);

    fprintf(fp, "core write [%05o] <= %05o\n", addr, data);
    return p;
}

static uint8_t *alloc_trace_record(pdp8_trace_t *trc, int bytes) {
    if (!TRACE_ENABLED(trc)) {
        return NULL;
    }
    
    /* always guarantee there's an extra byte if we need to 
     * store an out of memory record.
     */
    if (trc->used + bytes + 1 > trc->allocated) {
        uint8_t *realloced = realloc(trc->trace, 2 * trc->allocated);
        if (realloced == NULL) {
            assert(trc->used < trc->allocated);
            trc->trace[trc->used++] = trc_out_of_memory;
            trc->out_of_memory = 1;
            return NULL;            
        }

        trc->trace = realloced;
        trc->allocated *= 2;
    }

    /* we should never have a record so large that one realloc isn't enough */
    assert(trc->used + bytes + 1 <= trc->allocated);

    trc->used += bytes;
    return trc->trace + trc->used - bytes;
}
