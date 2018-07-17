/*
 * Priority queue event scheduler using binary heaps
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
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

static void filter_up(scheduler_t *sch, int n);    
static void filter_down(scheduler_t *sch, int n);    

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

    filter_up(sch, n);

    return 0;
}

int scheduler_delete(scheduler_t *sch, scheduler_event_t event, void *ctx) {
    int found = 0;

    /* note that we delete ALL matching events */
    for (int n = 1; n < sch->free_node; n++) {
        if (sch->nodes[n].event == event && sch->nodes[n].ctx == ctx) {
            if (n == sch->free_node - 1) {
                sch->free_node--;
                n = 1;
                continue;
            }

            sch->nodes[n] = sch->nodes[sch->free_node - 1];
            sch->free_node--;

            /* this is a min heap, so if this node is less than the parent, it neads to move up */
            if (n == 1 && sch->nodes[n].key > sch->nodes[n / 2].key) {
                filter_down(sch, n);
            } else {
                filter_up(sch, n);
            }
        }
    }

    return found ? 0 : -1;
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
    filter_down(sch, 1);

    return 0;
}

static void filter_up(scheduler_t *sch, int n) {
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
}

static void filter_down(scheduler_t *sch, int n) {
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
}

void scheduler_print(scheduler_t *sch) {
    printf("heap has %d entries\n", sch->free_node - 1);
    for (int i = 1; i < sch->free_node; i++) {
        printf("%d ", (int)sch->nodes[i].key);
    }
    printf("\n");
}
