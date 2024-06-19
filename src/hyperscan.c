#include <stdio.h>

#include "main.h"
#include <hs/hs.h>

static int found = 0;

static int eventHandler(UNUSED unsigned int  id,
                        UNUSED unsigned long long from,
                        UNUSED unsigned long long to,
                        UNUSED unsigned int flags,
                        UNUSED void * ctx) {
    fprintf(stdout, "Match for pattern \"%s\" at offset %llu\n", (char*)ctx, from);
    found++;
    return 0;
}

static int eventHandlerMulti(UNUSED unsigned int  id,
                        UNUSED unsigned long long from,
                        UNUSED unsigned long long to,
                        UNUSED unsigned int flags,
                        UNUSED void * ctx) {
    const char ** patterns = (const char**)ctx;
    fprintf(stdout, "%s Match for pattern \"%s\" at offset %llu\n", __func__, *(patterns+id), from);
    found++;
    return 0;
}

bool hs_verify_regex(const char* pattern) {
    hs_database_t * database;
    hs_compile_error_t * compile_err;
    if (hs_compile(pattern, 
                    HS_FLAG_DOTALL | HS_FLAG_MULTILINE | HS_FLAG_SOM_LEFTMOST,
                    // HS_FLAG_CASELESS, 
                    HS_MODE_BLOCK, 
                    NULL, 
                    &database, 
                    &compile_err) != HS_SUCCESS) {
        // fprintf(stderr, "ERROR: Unable to compile pattern \"%s\": %s\n",
        //         pattern, compile_err->message);
        hs_free_compile_error(compile_err);
        return false;
    }

    hs_free_database(database);
    return true;
}

int hs_find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * res)
{
    TIME_TYPE start, end;
    hs_database_t * database;

    double pre_times = 0;
    GET_TIME(start);

    hs_compile_error_t * compile_err;
    if (hs_compile(pattern, 
                    HS_FLAG_DOTALL | HS_FLAG_MULTILINE | HS_FLAG_SOM_LEFTMOST, 
                    HS_MODE_BLOCK, 
                    NULL, 
                    &database, 
                    &compile_err) != HS_SUCCESS) {
        fprintf(stderr, "ERROR: Unable to compile pattern \"%s\": %s\n",
                pattern, compile_err->message);
        hs_free_compile_error(compile_err);
        return -1;
    }

    hs_scratch_t * scratch = NULL;
    if (hs_alloc_scratch(database, &scratch) != HS_SUCCESS) {
        fprintf(stderr, "ERROR: Unable to allocate scratch space. Exiting.\n");
        hs_free_database(database);
        return -1;
    }

    GET_TIME(end);
    pre_times = TIME_DIFF_IN_MS(start, end);

    double * times = calloc(repeat, sizeof(double));
    int const times_len = repeat;

    do {
        found = 0;
        GET_TIME(start);
        if (hs_scan(database, subject, subject_len, 0, scratch, eventHandler, (void*)pattern) != HS_SUCCESS) {
            fprintf(stderr, "ERROR: Unable to scan input buffer. Exiting.\n");
            hs_free_scratch(scratch);
            hs_free_database(database);
            free(times);
            return -1;
        }
        GET_TIME(end);

        times[repeat - 1] = TIME_DIFF_IN_MS(start, end);
    } while (--repeat > 0);

    res->matches = found;
    get_mean_and_derivation(pre_times, times, times_len, res);

    hs_free_scratch(scratch);
    hs_free_database(database);
    free(times);

    return 0;
}

int hs_multi_find_all(const char ** pattern, int pattern_num, const char * subject, int subject_len
                        , int repeat, struct result * res)
{
    TIME_TYPE start, end;

    hs_database_t * database;
    hs_compile_error_t * compile_err;
    unsigned all_flags[MAX_RULES];
    unsigned all_rule_ids[MAX_RULES];
    for(int i = 0; i < pattern_num; i++)
    {
        all_flags[i] = HS_FLAG_DOTALL | HS_FLAG_MULTILINE | HS_FLAG_SOM_LEFTMOST;
        all_rule_ids[i] = i;
    }

    double pre_times = 0;
    GET_TIME(start);

    if (hs_compile_multi((const char *const *)pattern,
                         all_flags,
                         all_rule_ids, 
                         pattern_num, 
                         HS_MODE_BLOCK, 
                         NULL, 
                         &database, 
                         &compile_err) != HS_SUCCESS) {
        fprintf(stderr, "ERROR: Unable to compile patterns: '%s'\n", compile_err->message);
        hs_free_compile_error(compile_err);
        return -1;
    }

    hs_scratch_t * scratch = NULL;
    if (hs_alloc_scratch(database, &scratch) != HS_SUCCESS) {
        fprintf(stderr, "ERROR: Unable to allocate scratch space. Exiting.\n");
        hs_free_database(database);
        return -1;
    }

    GET_TIME(end);
    pre_times = TIME_DIFF_IN_MS(start, end);

    double * times = calloc(repeat, sizeof(double));
    int const times_len = repeat;

    do {
        found = 0;
        GET_TIME(start);
        printf("Scanning with ");
        for (int i = 0; i < pattern_num; i++)
        {
            printf("'%s',", *(pattern+i));
        }
        printf("\n");

        if (hs_scan(database, subject, subject_len, 0, scratch, eventHandlerMulti, pattern) != HS_SUCCESS) {
            fprintf(stderr, "ERROR: Unable to scan input buffer. Exiting.\n");
            hs_free_scratch(scratch);
            hs_free_database(database);
            free(times);
            return -1;
        }
        GET_TIME(end);

        times[repeat - 1] = TIME_DIFF_IN_MS(start, end);
    } while (--repeat > 0);

    res->matches = found;
    get_mean_and_derivation(pre_times, times, times_len, res);

    hs_free_scratch(scratch);
    hs_free_database(database);
    free(times);

    return 0;
}
