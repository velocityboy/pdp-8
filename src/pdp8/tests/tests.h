#ifndef _TESTS_H_
#define _TESTS_H_

typedef struct test_harness_t test_harness_t;

typedef void (*testfn)(int);
extern int register_test(testfn fn, char *name, char *desc);
extern void __test_cond_failed(int testid, char *cond, char *file, int line, char *desc);

#define DECLARE_TEST(name, desc) \
  static void __test_##name(int); \
  static int __testid_##name; \
  __attribute__((constructor)) static void __register_##name() { \
    __testid_##name = register_test(&__test_##name, #name, desc); \
  } \
  static void __test_##name(int _testid)

#define ASSERT(cond) ((void)((cond) ? (void)0 : __test_cond_failed(_testid, #cond, __FILE__, __LINE__, NULL)))
#define ASSERT_V(cond, desc) ((void)((cond) ? (void)0 : __test_cond_failed(_testid, #cond, __FILE__, __LINE__, desc)))
  
#endif
