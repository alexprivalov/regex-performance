#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include "main.h"
#include "version.h"

#include <vector>
#include <string>
#include <fstream>

struct engines {
    const char * name;
    int (*find_all)(const char* pattern, const char* subject, int subject_len, int repeat, struct result * result);
};

static struct engines engines [] = {
#ifdef INCLUDE_CTRE
    {.name = "ctre",        .find_all = ctre_find_all},
#endif
#ifdef INCLUDE_BOOST
    {.name = "boost",        .find_all = boost_find_all},
#endif
#ifdef INCLUDE_CPPSTD
    {.name = "cppstd",        .find_all = cppstd_find_all},
#endif
#ifdef INCLUDE_PCRE2
    {.name = "pcre",        .find_all = pcre2_std_find_all},
    {.name = "pcre-dfa",    .find_all = pcre2_dfa_find_all},
    {.name = "pcre-jit",    .find_all = pcre2_jit_find_all},
#endif
#ifdef INCLUDE_RE2
    {.name = "re2",         .find_all = re2_find_all},
#endif
// #ifdef INCLUDE_ONIGURUMA
//     {.name = "onig",        .find_all = onig_find_all},
// #endif
// #ifdef INCLUDE_TRE
//     {.name = "tre",         .find_all = tre_find_all},
// #endif
#ifdef INCLUDE_HYPERSCAN
    {.name = "hscan",       .find_all = hs_find_all},
#endif
#ifdef INCLUDE_YARA
    {.name = "yara",        .find_all = yara_find_all},
#endif
    {.name = "rust_regex",  .find_all = rust_find_all},
    {.name = "rust_regrs",  .find_all = regress_find_all},
};

// static char * regex [] = {
//     "Twain",
//     "(?i)Twain",
//     "[a-z]shing",
//     "Huck[a-zA-Z]+|Saw[a-zA-Z]+",
//     "\\b\\w+nn\\b",
//     "[a-q][^u-z]{13}x",
//     "Tom|Sawyer|Huckleberry|Finn",
//     "(?i)Tom|Sawyer|Huckleberry|Finn",
//     ".{0,2}(Tom|Sawyer|Huckleberry|Finn)",
//     ".{2,4}(Tom|Sawyer|Huckleberry|Finn)",
//     "Tom.{10,25}river|river.{10,25}Tom",
//     "[a-zA-Z]+ing",
//     "\\s[a-zA-Z]{0,12}ing\\s",
//     "([A-Za-z]awyer|[A-Za-z]inn)\\s",
//     "[\"'][^\"']{0,30}[?!\\.][\"']",
//     "\u221E|\u2713",
//     "\\p{Sm}",                               // any mathematical symbol
//     "(.*?,){13}z"
// };

std::string load(const char * file_name)
{
    std::string ret;

    do {
        std::ifstream file(file_name);
        if (!file.is_open()) {
            fprintf(stderr, "Cannot open '%s'!\n", file_name);
            break;
        }

        while (!file.eof()) {
            std::string line;
            std::getline(file, line);
            ret.append(line);
        }
    } while (false);

    return ret;
}

std::vector<std::string> loadRegex(char const * file_name)
{
    std::vector<std::string> regexes;

    do {
        std::ifstream file(file_name);
        if (!file.is_open()) {
            fprintf(stderr, "Cannot open '%s'!\n", file_name);
            break;
        }

        while (!file.eof()) {
            std::string line;
            std::getline(file, line);
            if (!line.empty() && line[0] != '#') {
                regexes.push_back(line);
            }
        }
    } while (false);

    return regexes;
}

void find_all(const char* pattern, const char* subject, int subject_len, int repeat, struct result * engine_results)
{
    fprintf(stdout, "-----------------\nRegex: '%s'\n", pattern);

    for (size_t iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
        int ret = engines[iter].find_all(pattern, subject, subject_len, repeat, &(engine_results[iter]));
        if (ret == -1) {
            engine_results[iter].pre_time = 0;
            engine_results[iter].time = 0;
            engine_results[iter].time_sd = 0;
            engine_results[iter].matches = 0;
            engine_results[iter].score = 0;
        } else {
            printResult(engines[iter].name, &(engine_results[iter]));
        }
    }

    int score_points = 5;
    for (int top = 0; top < score_points; top++) {
        double best = 0;

        for (size_t iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
            if (engine_results[iter].time > 0 &&
                engine_results[iter].score == 0 &&
                (best == 0 || best > engine_results[iter].time)) {
                best = engine_results[iter].time;
            }
        }

        for (size_t iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
            if (engine_results[iter].time > 0 && best == engine_results[iter].time) {
                engine_results[iter].score = score_points;
            }
        }

        score_points--;
    }

}

void get_mean_and_derivation(double pre_times, double * times, uint32_t times_len, struct result * res)
{
    double mean, sd, var, sum = 0.0, sdev = 0.0;

    if (times == NULL || res == NULL || times_len == 0) {
        return;
    }

    res->pre_time = pre_times;

    if (times_len == 1) {
        res->time = times[0];
        res->time_sd = 0;
    }

    /* get mean value */
    for (uint32_t iter = 0; iter < times_len; iter++) {
        sum += times[iter];
    }
    mean = sum / times_len;

    /* get variance */
    for (uint32_t iter = 0; iter < times_len; iter++) {
        sdev += (times[iter] - mean) * (times[iter] - mean);
    }
    var = sdev / (times_len - 1);

    /* get standard derivation */
    sd = sqrt(var);

    res->time = mean;
    res->time_sd = sd;
}

void printResult(const char * name, struct result * res)
{
    fprintf(stdout, "[%10s] pre_time: %7.4f ms, time: %7.1f ms (+/- %4.1f %%), matches: %8d\n", name, res->pre_time, res->time, (res->time_sd / res->time) * 100, res->matches);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    char const * file = NULL;
    char * out_file = NULL;
    char * input_regex = NULL;
    int repeat = 5;
    int mode = 0;
    int c = 0;

    while ((c = getopt(argc, argv, "n:m:i:hvf:o:")) != -1) {
        switch (c) {
            case 'f':
                file = optarg;
                break;
            case 'n':
                repeat = atoi(optarg);
                break;
            case 'm':
                mode = atoi(optarg);
                if ((mode != 0) && (mode != 1)) {
                    printf("Mode input error!\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'i':
                input_regex = optarg;
                break;
            break;   
            case 'o':
                out_file = optarg;
                break;
            case 'v':
                printf("%s\n", VERSION_STRING);
                exit(EXIT_SUCCESS);
            case 'h':
                printf("Usage: %s [option] -f <file> -i <input_regex_list> \n\n", argv[0]);
                printf("Options:\n");
                printf("  -f\tInput file.\n");
                printf("  -n\tSet number of repetitions. Default: 5\n");
                printf("  -m\tSet mode (0: regex one by one; 1: regex together). Default: 0\n");
                printf("  -i\tRead regex patterns from file.\n");
                printf("  -o\tWrite measured data into CSV file.\n");
                printf("  -v\tGet the application version and build date.\n");
                printf("  -h\tPrint this help message\n\n");
                exit(EXIT_SUCCESS);
        }
    }

    if (file == NULL) {
        fprintf(stderr, "No input file given.\n");
        exit(EXIT_FAILURE);
    }

    if (input_regex == NULL) {
        fprintf(stderr, "No input regex list given.\n");
        exit(EXIT_FAILURE);
    }

    auto regexes = loadRegex(input_regex);
    std::vector<const char *> regex;
    for (auto &regex_ : regexes) {
        fprintf(stdout, "Regex: %s\n", regex_.c_str());
        regex.push_back(regex_.c_str());
    }

    fprintf(stdout, "Total amount of records: %ld\n", regexes.size());

    auto data = load(file);
    if (data.empty()) {
        exit(EXIT_FAILURE);
    }

    if (mode == 0) {
        printf("\n[Match regex patterns one by one]\n\n");

        struct result results[MAX_RULES][sizeof(engines)/sizeof(engines[0])] = {0};
        struct result engine_results[sizeof(engines)/sizeof(engines[0])] = {0};

        for (size_t  iter = 0; iter < regex.size(); iter++) {
            find_all(regex[iter], data.c_str(), data.size(), repeat, results[iter]);

            for (size_t iiter = 0; iiter < sizeof(engines)/sizeof(engines[0]); iiter++) {
                engine_results[iiter].pre_time += results[iter][iiter].pre_time;
                engine_results[iiter].time += results[iter][iiter].time;
                engine_results[iiter].matches += results[iter][iiter].matches;
                engine_results[iiter].score += results[iter][iiter].score;
            }
        }

        fprintf(stdout, "-----------------\nTotal Results:\n");
        for (size_t iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
            fprintf(stdout, "[%10s] pre time: %7.4f ms | match time: %7.1f ms | matches: %8d | score: %6u points |\n", engines[iter].name, engine_results[iter].pre_time, engine_results[iter].time, engine_results[iter].matches, engine_results[iter].score);
        }

        if (out_file != NULL) {
            FILE * f;
            f = fopen(out_file, "w");
            if (!f) {
                fprintf(stderr, "Cannot open '%s'!\n", out_file);
                exit(EXIT_FAILURE);
            }

            /* write table header*/
            fprintf(f, "id;");
            fprintf(f, "regex;");
            for (size_t iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
                fprintf(f, "%s (pre) [ms];", engines[iter].name);
            }
            for (size_t iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
                fprintf(f, "%s (match) [ms];", engines[iter].name);
            }
            for (size_t iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
                fprintf(f, "%s [matches];", engines[iter].name);
            }
            for (size_t iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
                fprintf(f, "%s [sp];", engines[iter].name);
            }
            fprintf(f, "\n");

            /* write data */
            for (size_t iter = 0; iter < regex.size(); iter++) {
                fprintf(f, "%lu;", iter + 1);
                fprintf(f, "%s;", regex[iter]);

                for (size_t iiter = 0; iiter < sizeof(engines)/sizeof(engines[0]); iiter++) {
                    fprintf(f, "%7.4f;", results[iter][iiter].pre_time);
                }
                for (size_t iiter = 0; iiter < sizeof(engines)/sizeof(engines[0]); iiter++) {
                    fprintf(f, "%7.1f;", results[iter][iiter].time);
                }
                for (size_t iiter = 0; iiter < sizeof(engines)/sizeof(engines[0]); iiter++) {
                    fprintf(f, "%d;", results[iter][iiter].matches);
                }
                for (size_t iiter = 0; iiter < sizeof(engines)/sizeof(engines[0]); iiter++) {
                    fprintf(f, "%d;", results[iter][iiter].score);
                }
                fprintf(f, "\n");
            }

            fclose(f);
        }

    } else {
        printf("\n[Match regex patterns all together]\n\n");
        struct result *results;
        results = (struct result*)malloc(sizeof(struct result));
        if (hs_multi_find_all(regex.data(), regex.size(), data.c_str(), data.size(), repeat, results) == -1) {
            exit(EXIT_FAILURE);
        }
        printResult("hscan-multi", results);

        if (out_file != NULL) {

            FILE * f;
            f = fopen(out_file, "w");
            if (!f) {
                fprintf(stderr, "Cannot open '%s'!\n", out_file);
                exit(EXIT_FAILURE);
            }

            /* write table header*/
            fprintf(f, "id;");
            fprintf(f, "regex;");
            fprintf(f, "hs-multi (pre) [ms];");
            fprintf(f, "hs-multi (match) [ms];");
            fprintf(f, "hs-multi [matches];");
            fprintf(f, "\n");

            /* write data */
            fprintf(f, "%lu;", regex.size());
            fprintf(f, "%7.4f;", results->pre_time);
            fprintf(f, "%7.1f;", results->time);
            fprintf(f, "%d;", results->matches);
            fprintf(f, "\n");

            fclose(f);
        }
    }

    exit(EXIT_SUCCESS);
}
