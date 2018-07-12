/*
 * Priority queue event scheduler using binary heaps
 */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "scheduler.h"

#define INITIAL_NODES 16

typedef struct node_t node_t;

struct node_t {
    uint64_t key;
    void *ctx;
    scheduler_event_t event;
};

struct scheduler_t {
    node_t *nodes;
    int allocated;
    int free_node;
};

scheduler_t *scheduler_create() {
    scheduler_t *sch = (scheduler_t *)calloc(1, sizeof(scheduler_t));
    if (!sch) {
        return sch;
    }

    sch->nodes = (node_t *)calloc(INITIAL_NODES, sizeof(node_t));
    if (!sch->nodes) {
        free(sch);
        return NULL;
    }

    sch->allocated = INITIAL_NODES;
    sch->free_node = 1;

    return sch;
}

void scheduler_free(scheduler_t *sch) {
    if (sch) {
        free(sch->nodes);
        free(sch);
    }
}

int scheduler_insert(scheduler_t *sch, uint64_t time, scheduler_event_t event, void *ctx) {
    if (sch->free_node == sch->allocated) {
        int new_allocated = sch->allocated * 2;
        node_t *reallocated = (node_t *)realloc(sch->nodes, new_allocated * sizeof(node_t));
        if (reallocated == NULL) {
            return -1;
        }

        memset(&reallocated[sch->allocated], 0, (new_allocated - sch->allocated) * sizeof(node_t));
        sch->nodes = reallocated;
        sch->allocated = new_allocated;
    }

    int n = sch->free_node++;
    sch->nodes[n].key = time;
    sch->nodes[n].ctx = ctx;
    sch->nodes[n].event = event;

    while (n != 1) {
        int parent = n / 2;

        if (sch->nodes[n].key >= sch->nodes[parent].key) {
            break;
        }

        node_t temp = sch->nodes[n];
        sch->nodes[n] = sch->nodes[parent];
        sch->nodes[parent] = temp;

        n = parent;
    }

    return 0;
}

void scheduler_clear(scheduler_t *sch) {
    sch->free_node = 1;
}

int scheduler_empty(scheduler_t *sch) {
    return sch->free_node == 1;
}

uint64_t scheduler_next_event_time(scheduler_t *sch) {
    if (sch->free_node == 1) {
        return UINT64_MAX;
    }
    return sch->nodes[1].key;
}

int scheduler_extract_min(scheduler_t *sch, scheduler_callback_t *callback) {
    if (sch->free_node == 1) {
        return -1;
    }

    callback->time = sch->nodes[1].key;
    callback->event = sch->nodes[1].event;
    callback->ctx = sch->nodes[1].ctx;

    sch->nodes[1] = sch->nodes[--sch->free_node];

    int n = 1;

    while (1) {
        int left = 2 * n;
        int right = left + 1;

        uint64_t key = sch->nodes[n].key;

        int swap = n;
        if (left < sch->free_node && sch->nodes[left].key < key) {
            swap = left;
            key = sch->nodes[left].key;
        }
        
        if (right < sch->free_node && sch->nodes[right].key < key) {
            swap = right;
        }

        if (swap == n) {
            break;
        }

        node_t temp = sch->nodes[n];
        sch->nodes[n] = sch->nodes[swap];
        sch->nodes[swap] = temp;

        n = swap;
    }

    return 0;
}

