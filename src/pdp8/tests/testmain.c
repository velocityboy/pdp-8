#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tests.h"

typedef struct test_failure_t {
    char *cond;
    char *file;
    char *desc;
    int line;
} test_failure_t;

typedef struct test_t {
    testfn fn;
    char *name;
    char *desc;
    int fail_count;
    test_failure_t *failures;
} test_t;

struct test_harness_t {
    int test_count;
    int test_slots;
    test_t *tests;
};

static test_harness_t harness = { 0, 0, NULL };

typedef struct colors_t {
    char *red;
    char *green;
    char *reset;
} colors_t;

int register_test(testfn fn, char *name, char *desc) {
    if (harness.test_count == harness.test_slots) {
        int new_slots = harness.test_slots ? harness.test_slots * 2 : 16;
        harness.tests = (test_t*)realloc(harness.tests, new_slots * sizeof(test_t));
        harness.test_slots = new_slots;
    }

    int testid = harness.test_count++;
    test_t *test = &harness.tests[testid];

    test->fn = fn;
    test->name = name;
    test->desc = desc;
    test->fail_count = 0;
    test->failures = NULL;

    return testid;
}

void __test_cond_failed(int testid, char *cond, char *file, int line, char *desc) {
    test_t *test = &harness.tests[testid];
    test->failures = (test_failure_t *)realloc(test->failures, (test->fail_count + 1) * sizeof(test_failure_t));
    test_failure_t *failure = &test->failures[test->fail_count++];

    failure->cond = cond;
    failure->file = file;
    failure->line = line;

    /* other strings are guaranted static because they are from the compiler/preprocessor, but
     * 'desc' comes from the actual test code so strdup() in case it's a local.
     */
    failure->desc = desc ? strdup(desc) : NULL;
}

static int cmp_tests_by_name(const void *left, const void *right) {
    int cmp = strcmp(((test_t *)left)->name, ((test_t *)right)->name);
    if (cmp == 0) {
        cmp = strcmp(((test_t *)left)->desc, ((test_t *)right)->desc);
    }
    return cmp;
}

static void setup_colors(colors_t *colors) {
    if (isatty(fileno(stdout))) {
        colors->red = "\x1b[31m";
        colors->green = "\x1b[32m";
        colors->reset = "\x1b[0m";
    } else {
        colors->red = "";
        colors->green = "";
        colors->reset = "";
    }
}

static char *basename(char *path) {
    char *lastslash = strrchr(path, '/');
    if (lastslash == NULL) {
        return path;
    }
    return lastslash + 1;
}

static void help(char *name) {
    char *base = basename(name);
    printf("%s: [-f] [-h]\n", base);
    printf("\t-f\tOnly print failing tests\n");
    printf("\t-h\tShow this help\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    int only_failing = 0;
    int ch;

    while ((ch = getopt(argc, argv, "fh")) != -1) {
        switch (ch) {
            case 'f':
                only_failing++;
                break;

            default:
                help(argv[0]);
        }
    }

    qsort(harness.tests, harness.test_count, sizeof(test_t), &cmp_tests_by_name);

    colors_t colors;
    setup_colors(&colors);

    int passed = 0;
    
    for (int id = 0; id < harness.test_count; id++) {
        test_t *test = &harness.tests[id];
        test->fn(id);
        if (test->fail_count == 0) {
            if (!only_failing) {
                printf("[%sPASS%s] %s - %s\n", colors.green, colors.reset, test->name, test->desc);
            }
            passed++;
            continue;
        }

        printf("[%sFAIL%s] %s - %s\n", colors.red, colors.reset, test->name, test->desc);
        for (int i = 0; i < test->fail_count; i++) {
            test_failure_t *failure = &test->failures[i];
            printf("  %s@%d: %s", basename(failure->file), failure->line, failure->cond);
            if (failure->desc) {
                printf(" - %s", failure->desc);                
            }
            putchar('\n');
        }
    }

    printf("\n%d of %d tests passed.\n", passed, harness.test_count);
    return harness.test_count - passed;
}