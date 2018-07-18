#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pdp8/logger.h"

static char **categories = NULL;
static char *enabled = NULL;
static int cat_used = 0;
static int cat_allocated = 0;
static int cat_max_len = 0;
static FILE *fp = NULL;

int logger_get_category(char *category) {
    for (int i = 0; i < cat_used; i++) {
        if (strcasecmp(category, categories[i]) == 0) {
            return i;
        }
    }
    return -1;
}

int logger_add_category(char *category) {
    int id = logger_get_category(category);
    if (id >= 0) {
        return id;
    }

    /* always have one free slot so we can return 'categories' as a
     * NULL terminated list of strings
     */
     if (cat_used + 1 >= cat_allocated) {
        int new_allocated = cat_allocated ? 2 * cat_allocated : 16;
        char **new_categories = realloc(categories, new_allocated * sizeof(char*));
        char *new_enabled = realloc(enabled, new_allocated * sizeof(char));
        if (new_categories == NULL || new_enabled == NULL) {
            free(new_categories);
            free(new_enabled);
            return -1;
        }
        memset(new_categories + cat_used, 0, (new_allocated - cat_used) * sizeof(char*));
        cat_allocated = new_allocated;
        categories = new_categories;
        enabled = new_enabled;
    }

    categories[cat_used] = strdup(category);

    size_t len = strlen(category);
    if (len > cat_max_len) {
        cat_max_len = len;
    }

    return cat_used++;
}

char **logger_get_categories() {
    if (categories == NULL) {
        static char *empty[] = { NULL };
        return empty;
    }
    return categories;
}

void logger_enable_category(int cat, int enable) {
    if (cat >= 0 || cat < cat_used) {
        enabled[cat] = enable ? 1 : 0;
    }
}

int logger_set_file(char *fn) {    
    logger_close_file();
    return (fp = fopen(fn, "w")) ? 0 : -1;
}

void logger_close_file() {
    if (fp) {
        fclose(fp);
        fp = NULL;
    }
}

void logger_log(int cat, char *fmt, ...) {
    if (!fp || cat < 0 || cat >= cat_used || !enabled[cat]) {
        return;
    }

    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    fprintf(fp, "%s %-*s ", ts, cat_max_len, categories[cat]);
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    fprintf(fp, "\n");
}
