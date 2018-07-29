#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer/lin_buffer.h"

struct lin_buffer_t {
    lb_ptr_t alloc;
    lb_ptr_t used;
    uint8_t *buf;
    lb_ptr_t wrstart;             /* start and end of writable region */
    lb_ptr_t wrend;               /* in last allocated event */
    lb_ptr_t rdstart;
    lb_ptr_t rdend;
};

static const uint32_t sig = 0x4C423031;           /* LB01 */

lin_buffer_t *lb_create(size_t ini_bytes) {
    lin_buffer_t *lb = calloc(1, sizeof(lin_buffer_t));
    if (lb) {
        lb->alloc = (lb_ptr_t)ini_bytes;
        lb->buf = calloc(1, ini_bytes);
        if (!lb->buf) {
            free(lb);
            lb = NULL;
        }
    }

    return lb;
}

void lb_destroy(lin_buffer_t *lb) {
    if (lb) {
        free(lb->buf);
        free(lb);
    }
}

#define HDR_SIG (0)
#define HDR_SIZE (1)
#define HDR_CNT (2)

int lb_save(lin_buffer_t *lb, FILE *fp) {
    uint32_t hdr[HDR_CNT];
    hdr[HDR_SIG] = htonl(sig);
    hdr[HDR_SIZE] = htonl(lb->used);

    if (fwrite(hdr, sizeof(uint32_t), HDR_CNT, fp) != HDR_CNT) {
        return -1;
    }

    if (fwrite(lb->buf, 1, lb->used, fp) != lb->used) {
        return -1;
    }

    return 0;
}

lin_buffer_t *lb_load(FILE *fp) {
    uint32_t hdr[HDR_CNT];

    if (fread(hdr, sizeof(uint32_t), HDR_CNT, fp) != HDR_CNT) {
        return NULL;
    }

    for (int i = 0; i < HDR_CNT; i++) {
        hdr[i] = ntohl(hdr[i]);
    }

    if (hdr[HDR_SIG] != sig) {
        errno = EINVAL;
        return NULL;
    }

    lin_buffer_t *lb = calloc(1, sizeof(lin_buffer_t));
    if (!lb) {
        return NULL;
    }

    lb->alloc = lb->used = hdr[HDR_SIZE];

    lb->buf = malloc(lb->used);
    if (!lb->buf) {
        free(lb);
        return NULL;
    }

    if (fread(lb->buf, 1, lb->used, fp) != lb->used) {
        lb_destroy(lb);
        lb = NULL;
    }

    return lb;
}

lin_buffer_t *lb_load_from_memory(uint8_t *p, size_t *len) {
  if (*len < HDR_CNT * sizeof(uint32_t)) {
    return NULL;
  }
  
  uint32_t *hdr = (uint32_t *)p;
  *len -= HDR_CNT * sizeof(uint32_t);
  p += HDR_CNT * sizeof(uint32_t);

  for (int i = 0; i < HDR_CNT; i++) {
    hdr[i] = ntohl(hdr[i]);
  }
  
  if (hdr[HDR_SIG] != sig) {
    errno = EINVAL;
    return NULL;
  }
  
  lin_buffer_t *lb = calloc(1, sizeof(lin_buffer_t));
  if (!lb) {
    return NULL;
  }
  
  lb->alloc = lb->used = hdr[HDR_SIZE];
  
  lb->buf = malloc(lb->used);
  if (!lb->buf) {
    free(lb);
    return NULL;
  }
  
  if (*len < lb->alloc) {
    lb_destroy(lb);
    lb = NULL;
  }
  
  memcpy(lb->buf, p, lb->alloc);
  *len -= lb->alloc;

  return lb;
}

lb_ptr_t lb_alloc_event(lin_buffer_t *lb, uint8_t type, uint8_t bytes) {
    lb_ptr_t resize = lb->alloc;

    while (bytes + 2 > resize - lb->used) {
        resize *= 2;
    }

    if (resize != lb->alloc) {
        uint8_t *buf = realloc(lb->buf, resize);
        if (!buf) {
            return LB_NULL;
        }
        lb->alloc = resize;
        lb->buf = buf;
    }

    assert(bytes + 2 <= lb->alloc - lb->used);

    lb->buf[lb->used++] = type;
    lb->buf[lb->used++] = bytes;

    lb_ptr_t dp = lb->used;
    lb->used += bytes;

    lb->wrstart = dp;
    lb->wrend = lb->used;

    return dp;
}

lb_ptr_t lb_put_uint8(lin_buffer_t *lb, lb_ptr_t p, uint8_t value) {
    lb_ptr_t pend = p + sizeof(value);
    if (p == LB_NULL || p < lb->wrstart || pend > lb->wrend) {
        return LB_NULL;
    }

    lb->buf[p] = value;
    return pend;
}

lb_ptr_t lb_put_uint16(lin_buffer_t *lb, lb_ptr_t p, uint16_t value) {
    lb_ptr_t pend = p + sizeof(value);
    if (p == LB_NULL || p < lb->wrstart || p + sizeof(value) > lb->wrend) {
        return LB_NULL;
    }

    *(uint16_t *)(lb->buf + p) = value;
    return pend;
}

lb_ptr_t lb_put_uint32(lin_buffer_t *lb, lb_ptr_t p, uint32_t value) {
    lb_ptr_t pend = p + sizeof(value);
    if (p == LB_NULL || p < lb->wrstart || p + sizeof(value) > lb->wrend) {
        return LB_NULL;
    }

    *(uint32_t *)(lb->buf + p) = value;
    return pend;
}

lb_ptr_t lb_first_event(lin_buffer_t *lb, uint8_t *type, uint8_t *bytes) {
    if (!lb->used) {
        return LB_NULL;
    }

    lb_ptr_t p = 0;
    *type = lb->buf[p++];
    *bytes = lb->buf[p++];

    lb->rdstart = p;
    lb->rdend = p + *bytes;

    assert(lb->rdend <= lb->used);

    return p;
}

lb_ptr_t lb_next_event(lin_buffer_t *lb, lb_ptr_t cur_event, uint8_t *type, uint8_t *bytes) {
    if (cur_event < 2 || cur_event > lb->used - 2) {
        return LB_NULL;
    }

    lb_ptr_t p = cur_event + lb->buf[cur_event - 1];
    if (p > lb->used - 2) {
        return LB_NULL;
    }

    uint8_t ty = lb->buf[p++];
    uint8_t by = lb->buf[p++];

    if (p + by > lb->used) {
        return LB_NULL;
    }

    *type = ty;
    *bytes = by;

    lb->rdstart = p;
    lb->rdend = p + by;

    return p;
}

lb_ptr_t lb_get_uint8(lin_buffer_t *lb, lb_ptr_t p, uint8_t *value) {
    lb_ptr_t pend = p + sizeof(*value);
    if (p == LB_NULL || p < lb->rdstart || pend > lb->rdend) {
        return LB_NULL;
    }

    *value = lb->buf[p++];
    return p;
}

lb_ptr_t lb_get_uint16(lin_buffer_t *lb, lb_ptr_t p, uint16_t *value) {
    lb_ptr_t pend = p + sizeof(*value);
    if (p == LB_NULL || p < lb->rdstart || pend > lb->rdend) {
        return LB_NULL;
    }

    *value = *(uint16_t *)(lb->buf + p);
    p += sizeof(uint16_t);
    return p;
}

lb_ptr_t lb_get_uint32(lin_buffer_t *lb, lb_ptr_t p, uint32_t *value) {
    lb_ptr_t pend = p + sizeof(*value);
    if (p == LB_NULL || p < lb->rdstart || pend > lb->rdend) {
        return LB_NULL;
    }

    *value = *(uint32_t *)(lb->buf + p);
    p += sizeof(uint32_t);
    return p;
}
