#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define TIME_TYPE                   clock_t
#define GET_TIME(res)               { res = clock(); }
#define TIME_DIFF_IN_MS(begin, end) (((double) (end - begin)) * 1000 / CLOCKS_PER_SEC)
#define UNUSED __attribute__((unused))
#define MAX_RULES 1000
#define MAX_REGEX_LEN 1000

struct result {
    int score;
    double pre_time;
    double time;
    double time_sd;
    int matches;
};

void get_mean_and_derivation(double pre_times, double * times, uint32_t times_len, struct result * res);

#ifdef INCLUDE_CTRE
int ctre_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res);
#endif
#ifdef INCLUDE_BOOST
int boost_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res);
#endif
#ifdef INCLUDE_CPPSTD
int cppstd_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res);
#endif
#ifdef INCLUDE_PCRE2
int pcre2_std_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res);
int pcre2_dfa_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res);
int pcre2_jit_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res);
#endif
#ifdef INCLUDE_RE2
int re2_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res);
#endif
#ifdef INCLUDE_TRE
int tre_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res);
#endif
#ifdef INCLUDE_ONIGURUMA
int onig_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res);
#endif
#ifdef INCLUDE_HYPERSCAN
#include <stdbool.h>
bool hs_verify_regex(const char* pattern);
int hs_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res);
int hs_multi_find_all(const char ** pattern, int pattern_num, const char * subject, int subject_len, int repeat, struct result * res);
#endif
#ifdef INCLUDE_YARA
int yara_find_all(char * pattern, char * subject, int subject_len, int repeat, struct result * res);
#endif
int rust_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res);
int regress_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res);

#ifdef __cplusplus
}
#endif
