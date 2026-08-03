/* Minimal test harness. No framework, no dependencies.
   A test is a function; assertions report and count. */
#ifndef RZ_HARNESS_H
#define RZ_HARNESS_H

#include <stdio.h>
#include <math.h>

static int rz_checks = 0;
static int rz_failures = 0;
static const char *rz_current = "(none)";

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { rz_current = #name; name(); } \
    static void name(void)

#define RUN(name) run_##name()

#define CHECK(cond) do { \
    rz_checks++; \
    if (!(cond)) { \
        rz_failures++; \
        printf("  FAIL %s:%d  %s\n      in %s\n", \
               __FILE__, __LINE__, #cond, rz_current); \
    } \
} while (0)

#define CHECK_EQ_U(actual, expected) do { \
    rz_checks++; \
    unsigned long long a_ = (unsigned long long)(actual); \
    unsigned long long e_ = (unsigned long long)(expected); \
    if (a_ != e_) { \
        rz_failures++; \
        printf("  FAIL %s:%d  %s\n      expected %llu, got %llu (in %s)\n", \
               __FILE__, __LINE__, #actual, e_, a_, rz_current); \
    } \
} while (0)

#define CHECK_NEAR(actual, expected, tol) do { \
    rz_checks++; \
    double a_ = (double)(actual); \
    double e_ = (double)(expected); \
    if (!(fabs(a_ - e_) <= (tol))) { \
        rz_failures++; \
        printf("  FAIL %s:%d  %s\n      expected %.6f +/- %.6f, got %.6f (in %s)\n", \
               __FILE__, __LINE__, #actual, e_, (double)(tol), a_, rz_current); \
    } \
} while (0)

static int rz_report(void)
{
    if (rz_failures == 0) {
        printf("PASS  %d checks\n", rz_checks);
        return 0;
    }
    printf("FAIL  %d of %d checks failed\n", rz_failures, rz_checks);
    return 1;
}

#endif
