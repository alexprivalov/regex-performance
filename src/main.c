#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include "main.h"
#include "version.h"

static char* data = NULL;
static int data_len = 0;

static char ** regex = NULL;
static int regex_num = 0;

struct engines {
    char * name;
    int (*find_all)(char* pattern, char* subject, int subject_len, int repeat, struct result * result);
};

static struct engines engines [] = {
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
    {.name = "rust_regex",  .find_all = rust_find_all},
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
//     "\\p{Sm}"                               // any mathematical symbol
// };

void load(char const * file_name)
{
    int i;

    FILE * f;
    f = fopen(file_name, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open '%s'!\n", file_name);
        return;
    }

    fseek(f, 0, SEEK_END);
    data_len = ftell(f);
    fseek(f, 0, SEEK_SET);

    data = (char*)malloc(data_len + 1);
    if (!data) {
        fprintf(stderr, "Cannot allocate memory!\n");
        fclose(f);
        return;
    }
    data[data_len] = '\0';

    int size = fread(data, data_len, 1, f);
    if (size == 0) {
        fprintf(stderr, "Reading file failed!\n");
    }
    fclose(f);

    for (i = 0; i < data_len; ++i) {
        if (data[i] == '\r') {
            data[i] = '\n';
        }
    }
    fprintf(stdout, "Input data '%s' loaded. (Length: %d bytes)\n", file_name, data_len);
}


void read_regex(char const * file_name)
{
    FILE * f;
    f = fopen(file_name, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open '%s'!\n", file_name);
        return;
    }
    
    regex = (char**)malloc(sizeof(char*) * MAX_RULES);
    if (!regex) {
        fprintf(stderr, "Cannot allocate memory!\n");
        fclose(f);
        return;
    }

	rewind(f);
    char *re = malloc( MAX_REGEX_LEN * sizeof( char ) );
    if (!re) {
        fprintf(stderr, "Cannot allocate memory!\n");
        fclose(f);
        return;
    }

	int i = 0, j = 0;
	unsigned int c = fgetc(f);

	//parsing the RegEx
	while(c != EOF){
		if (c == '\n' || c =='\r'){
			if(i != 0){
				re[i] = '\0';
				if (re[0] != '#'){
                    // parse_re(nfa, re);
                    regex[j] = malloc( MAX_REGEX_LEN * sizeof( char ) );
                    strcpy(regex[j], re);
                    j++;
                    fprintf(stdout, "[%4d]\t%s\n", j, re);
				} 
				i = 0;
				free(re);
                re = malloc( MAX_REGEX_LEN * sizeof( char ) );
                if (!re) {
                    fprintf(stderr, "Cannot allocate memory!\n");
                    fclose(f);
                    return;
                }
			}
		}else{
			re[i++] = c;
		}	
		c=fgetc(f);
	} //end while
	
	if(i != 0){
		re[i] = '\0';
		if (re[0] != '#'){
            // parse_re(nfa,re);
            regex[j] = malloc( MAX_REGEX_LEN * sizeof( char ) );
            strcpy(regex[j], re);
            fprintf(stdout, "[%4d]\t%s\n", ++j, re);
		}
		free(re);
    }
    
    regex_num = j;
    fprintf(stdout, "Regex ruleset '%s' loaded. (Total: %d regexes)\n", file_name, regex_num);
	// if (re!=NULL) free(re);
    fclose(f);

}

void find_all(char* pattern, char* subject, int subject_len, int repeat, struct result * engine_results)
{
    int iter;

    fprintf(stdout, "-----------------\nRegex: '%s'\n", pattern);

    for (iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
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

        for (iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
            if (engine_results[iter].time > 0 &&
                engine_results[iter].score == 0 &&
                (best == 0 || best > engine_results[iter].time)) {
                best = engine_results[iter].time;
            }
        }

        for (iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
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
    int32_t iter;

    if (times == NULL || res == NULL || times_len == 0) {
        return;
    }

    res->pre_time = pre_times;

    if (times_len == 1) {
        res->time = times[0];
        res->time_sd = 0;
    }

    /* get mean value */
    for (iter = 0; iter < times_len; iter++) {
        sum += times[iter];
    }
    mean = sum / times_len;

    /* get variance */
    for (iter = 0; iter < times_len; iter++) {
        sdev += (times[iter] - mean) * (times[iter] - mean);
    }
    var = sdev / (times_len - 1);

    /* get standard derivation */
    sd = sqrt(var);

    res->time = mean;
    res->time_sd = sd;
}

void printResult(char * name, struct result * res)
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

    read_regex(input_regex);
    if (regex_num == 0) {
        exit(EXIT_FAILURE);
    }

    load(file);
    if (data_len == 0) {
        exit(EXIT_FAILURE);
    }

    if (mode == 0) {
        printf("\n[Match regex patterns one by one]\n\n");

        struct result results[MAX_RULES][sizeof(engines)/sizeof(engines[0])] = {0};
        struct result engine_results[sizeof(engines)/sizeof(engines[0])] = {0};

        for (int iter = 0; iter < regex_num; iter++) {
            find_all(regex[iter], data, data_len, repeat, results[iter]);

            for (int iiter = 0; iiter < sizeof(engines)/sizeof(engines[0]); iiter++) {
                engine_results[iiter].pre_time += results[iter][iiter].pre_time;
                engine_results[iiter].time += results[iter][iiter].time;
                engine_results[iiter].matches += results[iter][iiter].matches;
                engine_results[iiter].score += results[iter][iiter].score;
            }
        }

        fprintf(stdout, "-----------------\nTotal Results:\n");
        for (int iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
            fprintf(stdout, "[%10s] pre time: %7.4f ms | match time: %7.1f ms | matches: %8d | score: %6u points |\n", engines[iter].name, engine_results[iter].pre_time, engine_results[iter].time, engine_results[iter].matches, engine_results[iter].score);
        }

        if (out_file != NULL) {
            int iter, iiter;

            FILE * f;
            f = fopen(out_file, "w");
            if (!f) {
                fprintf(stderr, "Cannot open '%s'!\n", out_file);
                exit(EXIT_FAILURE);
            }

            /* write table header*/
            fprintf(f, "regex;");
            for (iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
                fprintf(f, "%s [ms];", engines[iter].name);
            }
            for (iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
                fprintf(f, "%s [sp];", engines[iter].name);
            }
            for (iter = 0; iter < sizeof(engines)/sizeof(engines[0]); iter++) {
                fprintf(f, "%s [matches];", engines[iter].name);
            }
            fprintf(f, "\n");

            /* write data */
            for (iter = 0; iter < regex_num; iter++) {
                fprintf(f, "%s;", regex[iter]);

                for (iiter = 0; iiter < sizeof(engines)/sizeof(engines[0]); iiter++) {
                    fprintf(f, "%7.1f;", results[iter][iiter].time);
                }
                for (iiter = 0; iiter < sizeof(engines)/sizeof(engines[0]); iiter++) {
                    fprintf(f, "%d;", results[iter][iiter].score);
                }
                for (iiter = 0; iiter < sizeof(engines)/sizeof(engines[0]); iiter++) {
                    fprintf(f, "%d;", results[iter][iiter].matches);
                }
                fprintf(f, "\n");
            }

            fclose(f);
        }

    } else {
        printf("\n[Match regex patterns all together]\n\n");
        struct result *results;
        results = (struct result*)malloc(sizeof(struct result));
        hs_multi_find_all(regex, regex_num, data, data_len, repeat, results);
        printResult("hscan-multi", results);
    }

    free(data);
    free(regex);

    exit(EXIT_SUCCESS);
}
