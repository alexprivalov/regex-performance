#include <stdio.h>

#include "main.h"

#include <rregex.h>

int rust_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res)
{
    TIME_TYPE start, end;
    int found = 0;

    double pre_times = 0;
    GET_TIME(start);

    struct Regex const * regex_hdl = regex_new(pattern);
    if (regex_hdl == NULL) {
        fprintf(stderr, "ERROR: Unable to compile pattern \"%s\"\n", pattern);
        return -1;
    }

    GET_TIME(end);
    pre_times = TIME_DIFF_IN_MS(start, end);

    double * times = calloc(repeat, sizeof(double));
    int const times_len = repeat;

    do {
        GET_TIME(start);
        found = regex_matches(regex_hdl, (uint8_t*) subject, subject_len);
        GET_TIME(end);

        times[repeat - 1] = TIME_DIFF_IN_MS(start, end);

    } while (--repeat > 0);

    res->matches = found;
    get_mean_and_derivation(pre_times, times, times_len, res);

    regex_free(regex_hdl);
    free(times);

    return 0;
}

int regress_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result *res)
{
    TIME_TYPE start, end;
    int found = 0;

    struct Regress const *regex_hdl = regress_new(pattern);
    if (regex_hdl == NULL)
    {
        fprintf(stderr, "ERROR: Unable to compile pattern \"%s\"\n", pattern);
        return -1;
    }

    double *times = calloc(repeat, sizeof(double));
    int const times_len = repeat;

    do
    {
        GET_TIME(start);
        found = regress_matches(regex_hdl, (uint8_t *)subject, subject_len);
        GET_TIME(end);

        times[repeat - 1] = TIME_DIFF_IN_MS(start, end);

    } while (--repeat > 0);

    res->matches = found;
    get_mean_and_derivation(0, times, times_len, res);

    regress_free(regex_hdl);
    free(times);

    return 0;
}
