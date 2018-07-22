#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdp8_trace.h"

const uint8_t TRACE_VERSION = 2;
const uint32_t INITIAL_TRACE_BUFFER_SIZE = 1024;
const uint32_t MIN_BUFFER = INITIAL_TRACE_BUFFER_SIZE;

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
    uint32_t cap;
    uint32_t first_instr;
    uint32_t next_instr;
    uint32_t core_size;
    int out_of_memory;
    int locked;
    trace_reg_values_t initial_regs;
};

#define TRACE_ENABLED(trc) (!((trc)->out_of_memory || (trc)->locked))
#define TRACE_CIRCULAR_BUF(trc) ((trc)->cap != 0)

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

static uint32_t alloc_trace_record(pdp8_trace_t *trc, trace_type_t type, int bytes);

static int will_wrap(pdp8_trace_t *trc, uint32_t index, size_t bytes);
static inline uint32_t inc_index(pdp8_trace_t *trc, uint32_t index);
static inline uint32_t put_uint8(pdp8_trace_t *trc, uint32_t index, uint8_t data);
static inline uint32_t get_uint8(pdp8_trace_t *trc, uint32_t index, uint8_t *data);
static inline uint32_t put_uint16(pdp8_trace_t *trc, uint32_t index, uint16_t data);
static inline uint32_t put_uint32(pdp8_trace_t *trc, uint32_t index, uint32_t data);
static inline uint32_t get_uint16(pdp8_trace_t *trc, uint32_t index, uint16_t *data);
static inline uint32_t get_uint32(pdp8_trace_t *trc, uint32_t index, uint32_t *data);

pdp8_trace_t *pdp8_trace_create(struct pdp8_t *pdp8, uint32_t buffer_size) {
    pdp8_trace_t *trc = calloc(1, sizeof(pdp8_trace_t));
    if (trc == NULL) {
        return NULL;
    }

    if (buffer_size != TRACE_UNLIMITED && buffer_size < MIN_BUFFER) {
        buffer_size = MIN_BUFFER;
    }

    trc->trace = malloc(buffer_size == TRACE_UNLIMITED ? INITIAL_TRACE_BUFFER_SIZE : buffer_size);
    if (trc->trace == NULL) {
        pdp8_trace_free(trc);
        return NULL;
    }

    trc->pdp8 = pdp8;
    trc->allocated = INITIAL_TRACE_BUFFER_SIZE;
    trc->cap = buffer_size;
    trc->first_instr = 0;
    trc->next_instr = 0;
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
        free(trc->trace);
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
    uint32_t rec = alloc_trace_record(trc, trc_begin_instruction, 3 * sizeof(uint16_t));

    if (rec != UINT32_MAX) {
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

    uint32_t rec = alloc_trace_record(trc, trc_end_instruction, 8 * sizeof(uint16_t));

    if (rec != UINT32_MAX) {
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

    uint32_t rec = alloc_trace_record(trc, trc_memory_changed, 2 * sizeof(uint16_t));
    if (rec != UINT32_MAX) {
        rec = put_uint16(trc, rec, addr);
        rec = put_uint16(trc, rec, data);
    }
}

int pdp8_trace_save(pdp8_trace_t *trc, char *fn) {
    FILE *fp = fopen(fn, "wb");
    if (!fp) {
        return -1;
    }

    uint8_t header[1 + 4 * sizeof(uint32_t) + 8 * sizeof(uint16_t)];
    uint8_t *p = header;

    *p++ = TRACE_VERSION;
    /* how much to read back. don't save unused space at the end of a linear trace. */
    *(uint32_t *)p = htonl(TRACE_CIRCULAR_BUF(trc) ? trc->allocated : trc->next_instr);
    p += sizeof(uint32_t);
    *(uint32_t *)p = htonl(trc->first_instr);
    p += sizeof(uint32_t);
    *(uint32_t *)p = htonl(trc->next_instr);
    p += sizeof(uint32_t);
    *(uint32_t *)p = htonl(trc->core_size);
    p += sizeof(uint32_t);

    uint16_t bitregs = 0;
    if (trc->initial_regs.link) bitregs |= (1 << treg_link);
    if (trc->initial_regs.eae_mode_b) bitregs |= (1 << treg_eae_mode_b);
    if (trc->initial_regs.gt) bitregs |= (1 << treg_gt);

    *(uint16_t *)p = htons(bitregs);
    p+= sizeof(uint16_t);
    *(uint16_t *)p = htons(trc->initial_regs.ac);
    p+= sizeof(uint16_t);
    *(uint16_t *)p = htons(trc->initial_regs.sr);
    p+= sizeof(uint16_t);
    *(uint16_t *)p = htons(trc->initial_regs.mq);
    p+= sizeof(uint16_t);
    *(uint16_t *)p = htons(trc->initial_regs.sc);
    p+= sizeof(uint16_t);
    *(uint16_t *)p = htons(trc->initial_regs.ifr);
    p+= sizeof(uint16_t);
    *(uint16_t *)p = htons(trc->initial_regs.dfr);
    p+= sizeof(uint16_t);
    *(uint16_t *)p = htons(trc->initial_regs.ibr);
    p+= sizeof(uint16_t);
    assert(p == header + sizeof(header));

    if (fwrite(header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    int ok;
    if (TRACE_CIRCULAR_BUF(trc)) {
        ok = fwrite(trc->trace, trc->allocated, 1, fp) == 1;
    } else {
        ok = fwrite(trc->trace, trc->next_instr, 1, fp) == 1;
    }
        
    if (!ok) {
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

    uint8_t header[1 + 4 * sizeof(uint32_t) + 8 * sizeof(uint16_t)];
    uint8_t *p = header;

    if (fread(p, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    if (*p++ != TRACE_VERSION) {
        fclose(fp);
        return NULL;
    }    

    pdp8_trace_t *trc = calloc(1, sizeof(pdp8_trace_t));
    if (trc == NULL) {
        fclose(fp);
        return trc;
    }

    trc->allocated = ntohl(*(uint32_t *)p);
    p += sizeof(uint32_t);
    trc->first_instr = ntohl(*(uint32_t *)p);
    p += sizeof(uint32_t);
    trc->next_instr = ntohl(*(uint32_t *)p);
    p += sizeof(uint32_t);
    trc->core_size = ntohl(*(uint32_t *)p);
    p += sizeof(uint32_t);

    uint16_t bitregs;
    bitregs = ntohs(*(uint32_t *)p);
    p += sizeof(uint16_t);

    trc->initial_regs.link = (bitregs >> treg_link) & 1;
    trc->initial_regs.eae_mode_b = (bitregs >> treg_eae_mode_b) & 1;
    trc->initial_regs.gt = (bitregs >> treg_gt) & 1;

    trc->initial_regs.ac = ntohs(*(uint16_t *)p);
    p += sizeof(uint16_t);
    trc->initial_regs.sr = ntohs(*(uint16_t *)p);
    p += sizeof(uint16_t);
    trc->initial_regs.mq = ntohs(*(uint16_t *)p);
    p += sizeof(uint16_t);
    trc->initial_regs.sc = ntohs(*(uint16_t *)p);
    p += sizeof(uint16_t);
    trc->initial_regs.ifr = ntohs(*(uint16_t *)p);
    p += sizeof(uint16_t);
    trc->initial_regs.dfr = ntohs(*(uint16_t *)p);
    p += sizeof(uint16_t);
    trc->initial_regs.ibr = ntohs(*(uint16_t *)p);
    p += sizeof(uint16_t);

    assert(p == header + sizeof(header));

    uint32_t cnt = TRACE_CIRCULAR_BUF(trc) ? trc->allocated : trc->next_instr;

    int ok = 0;
    trc->trace = malloc(cnt);
    if (trc->trace) {
        ok = fread(trc->trace, cnt, 1, fp) == 1;
    }
    fclose(fp);

    if (!ok) {
        pdp8_trace_free(trc);
        return NULL;
    }

    /* loaded traces cannot be modified */
    trc->locked = 1;

    return trc;
}

static void list_begin_instruction(pdp8_trace_t *trc, uint32_t p, FILE *fp, trace_reg_values_t *reg);
static void list_end_instruction(pdp8_trace_t *trc, uint32_t p, FILE *fp, trace_reg_values_t *reg);
static void list_memory_changed(pdp8_trace_t *trc, uint32_t p, FILE *fp);

int pdp8_trace_save_listing(pdp8_trace_t *trc, char *fn) {
    FILE *fp = fopen(fn, "wb");
    if (!fp) {
        return -1;
    }

    fprintf(fp, "system has %d words of core\n", (int)trc->core_size);

    uint32_t p = trc->first_instr;
    uint32_t pend = trc->next_instr;

    trace_reg_values_t regs = trc->initial_regs;
    
    int done = 0;

    while (!done) {
        if (p == pend) {
            break;
        }

        trace_type_t type = trc->trace[p];
        p = inc_index(trc, p);
        if (p == pend) {
            break;
        }

        uint8_t bytes = trc->trace[p];
        p = inc_index(trc, p);
        if (p == pend) {
            break;
        }
        
        /* check for incomplete data */
        int tr = 0;
        if (TRACE_CIRCULAR_BUF(trc) && will_wrap(trc, p, bytes)) {            
            if (p + bytes - trc->allocated > trc->next_instr) {
                tr++;
            }
        } else if (p + bytes > trc->next_instr) {
            tr++;
        }
        if (tr) {
            fprintf(fp, "\ntrace truncated\n");
            break;
        }

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

        p += bytes;
        if (TRACE_CIRCULAR_BUF(trc)) {
            p %= trc->allocated;
        }
    }

    fclose(fp);
    return 0;
}

static const char *truncated = "\ntrace truncated\n";

static void list_begin_instruction(pdp8_trace_t *trc, uint32_t p, FILE *fp, trace_reg_values_t *regs) {
    uint16_t pc;
    p = get_uint16(trc, p, &pc);

    uint16_t ops[2];
    p = get_uint16(trc, p, ops);
    p = get_uint16(trc, p, ops+1);

    char decoded[200];
    pdp8_disassemble(pc, ops, regs->eae_mode_b, decoded, sizeof(decoded));
    fprintf(fp, "%s\n", decoded);
}

static void list_end_instruction(pdp8_trace_t *trc, uint32_t p, FILE *fp, trace_reg_values_t *iregs) {
    trace_reg_values_t regs;

    uint16_t bitregs;
    p = get_uint16(trc, p, &bitregs);       /* 1 */
    p = get_uint16(trc, p, &regs.ac);       /* 2 */
    p = get_uint16(trc, p, &regs.sr);       /* 3 */
    p = get_uint16(trc, p, &regs.mq);       /* 4 */
    p = get_uint16(trc, p, &regs.sc);       /* 5 */
    p = get_uint16(trc, p, &regs.ifr);      /* 6 */
    p = get_uint16(trc, p, &regs.dfr);      /* 7 */
    p = get_uint16(trc, p, &regs.ibr);      /* 8 */

    regs.link = (bitregs >> treg_link) & 1;
    regs.eae_mode_b = (bitregs >> treg_eae_mode_b) & 1;
    regs.gt = (bitregs >> treg_gt) & 1;
 
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

static void list_memory_changed(pdp8_trace_t *trc, uint32_t p, FILE *fp) {
    uint16_t addr;
    uint16_t data;

    p = get_uint16(trc, p, &addr);
    p = get_uint16(trc, p, &data);

    fprintf(fp, "core write [%05o] <= %05o\n", addr, data);
}

static int circular_overflow(pdp8_trace_t *trc, int bytes) {
    if (!will_wrap(trc, trc->next_instr, bytes)) {
        return trc->next_instr + bytes >= trc->first_instr;
    }

    return trc->next_instr + bytes - trc->allocated >= trc->first_instr;
}

static void remove_first_event(pdp8_trace_t *trc) {    
    uint32_t p = trc->first_instr;
    uint8_t type;
    uint8_t cnt;
    p = get_uint8(trc, p, &type);
    p = get_uint8(trc, p, &cnt);

    /* if we're deleting an end of instruction event, we're losing
     * register data, so we need to update the initial registers.
     */
    if (type == trc_end_instruction) {
        uint16_t bitregs;
        p = get_uint16(trc, p, &bitregs);       
        p = get_uint16(trc, p, &trc->initial_regs.ac);       
        p = get_uint16(trc, p, &trc->initial_regs.sr);       
        p = get_uint16(trc, p, &trc->initial_regs.mq);       
        p = get_uint16(trc, p, &trc->initial_regs.sc);       
        p = get_uint16(trc, p, &trc->initial_regs.ifr);      
        p = get_uint16(trc, p, &trc->initial_regs.dfr);      
        p = get_uint16(trc, p, &trc->initial_regs.ibr);      
    
        trc->initial_regs.link = (bitregs >> treg_link) & 1;
        trc->initial_regs.eae_mode_b = (bitregs >> treg_eae_mode_b) & 1;
        trc->initial_regs.gt = (bitregs >> treg_gt) & 1;                
    }

    p += cnt;
    if (TRACE_CIRCULAR_BUF(trc)) {
        p %= trc->allocated;
    }

    trc->first_instr = p;
}

static uint32_t alloc_trace_record(pdp8_trace_t *trc, trace_type_t type, int bytes) {
    if (!TRACE_ENABLED(trc)) {
        return UINT32_MAX;
    }
 
    if (TRACE_CIRCULAR_BUF(trc)) {
        while (circular_overflow(trc, bytes + 2)) {
            remove_first_event(trc);
        }

        uint32_t p = trc->next_instr;
        trc->next_instr = (trc->next_instr + bytes + 2) % trc->allocated;

        p = put_uint8(trc, p, type);
        p = put_uint8(trc, p, bytes);

        return p;
    } else {
        /* always guarantee we have two bytes left over for an out of
         * memory event
         */
        if (trc->next_instr + bytes + 4 > trc->allocated) {
            uint32_t new_alloc = 2 * trc->allocated;
            uint8_t *realloced = realloc(trc->trace, new_alloc);
            if (realloced == NULL) {
                assert(trc->next_instr <= trc->allocated - 2);
                trc->trace[trc->next_instr++] = trc_out_of_memory;
                trc->trace[trc->next_instr++] = 0;
                trc->out_of_memory = 1;
                return UINT32_MAX;            
            }

            trc->trace = realloced;
            trc->allocated  = new_alloc;
        }

        /* in an unlimited buffer, we should never have a record so large that one realloc isn't enough */
        assert(trc->next_instr + bytes + 4 <= trc->allocated);

        uint32_t p = trc->next_instr;

        p = put_uint8(trc, p, type);
        p = put_uint8(trc, p, bytes);
        trc->next_instr = p + bytes;

        return p;
    }
}

static int will_wrap(pdp8_trace_t *trc, uint32_t index, size_t bytes) {
    if (!TRACE_CIRCULAR_BUF(trc)) {
        return 0;
    }

    return index + bytes < trc->allocated;
}

static inline uint32_t inc_index(pdp8_trace_t *trc, uint32_t index) {
    index++;
    if (TRACE_CIRCULAR_BUF(trc)) {
        return index % trc->allocated;
    }
    return index;
}

static inline uint32_t put_uint8(pdp8_trace_t *trc, uint32_t index, uint8_t data) {
    trc->trace[index] = data;
    return inc_index(trc, index);
}

static inline uint32_t get_uint8(pdp8_trace_t *trc, uint32_t index, uint8_t *data) {
    *data = trc->trace[index];
    return inc_index(trc, index);
}

static inline uint32_t put_uint16(pdp8_trace_t *trc, uint32_t index, uint16_t data) {
    if (!will_wrap(trc, index, sizeof(uint16_t))) {
        *(uint16_t*)(trc->trace + index) = htons(data);
        return index + sizeof(uint16_t);
    }
    index = put_uint8(trc, index, (data >> 8) & 0377);
    index = put_uint8(trc, index, data & 0377);
    return index;
}

static inline uint32_t put_uint32(pdp8_trace_t *trc, uint32_t index, uint32_t data) {
    if (!will_wrap(trc, index, sizeof(uint32_t))) {
        *(uint32_t*)(trc->trace + index) = htonl(data);
        return index + sizeof(uint32_t);
    }
    index = put_uint8(trc, index, (data >> 24) & 0377);
    index = put_uint8(trc, index, (data >> 16) & 0377);
    index = put_uint8(trc, index, (data >> 8) & 0377);
    index = put_uint8(trc, index, data & 0377);

    return index;
}

static inline uint32_t get_uint16(pdp8_trace_t *trc, uint32_t index, uint16_t *data) {
    if (!will_wrap(trc, index, sizeof(uint16_t))) {
        *data = ntohs(*(uint16_t*)(trc->trace + index));
        return index + sizeof(uint16_t);
    }

    uint8_t t[2];
    index = get_uint8(trc, index, t+0);
    index = get_uint8(trc, index, t+1);

    *data = (t[0] << 8) | t[1];

    return index;    
}

static inline uint32_t get_uint32(pdp8_trace_t *trc, uint32_t index, uint32_t *data) {
    if (!will_wrap(trc, index, sizeof(uint32_t))) {
        *data = htonl(*(uint32_t*)(trc->trace + index));
        return index + sizeof(uint32_t);
    }

    uint8_t t[4];
    index = get_uint8(trc, index, t+0);
    index = get_uint8(trc, index, t+1);
    index = get_uint8(trc, index, t+2);
    index = get_uint8(trc, index, t+3);
    
    *data = (t[0] << 24) | (t[1] << 16) | (t[2] << 8) | t[3];

    return index;    
}
