#include "app_config.h"
#include "regex.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_pretty_print(const char *value_str, int *pretty_print) {
    char *end = NULL;
    errno = 0;
    long value = strtol(value_str, &end, 10);
    if (value_str[0] == '\0' || end == NULL || *end != '\0' || errno == ERANGE || value < 0 || value > INT_MAX) {
        fprintf(stderr, "Pretty print indent must be an integer number of spaces between 0 and %d\n", INT_MAX);
        return false;
    }

    *pretty_print = (int) value;
    return true;
}

static AnswerStructureType *parse_answer_structure(const char *value_str) {
    if (strcmp(value_str, "nested") == 0) return ANSWER_STRUCTURE_NESTED;
    if (strcmp(value_str, "flat") == 0) return ANSWER_STRUCTURE_FLAT;
    if (strcmp(value_str, "basic") == 0) return ANSWER_STRUCTURE_BASIC;

    fprintf(stderr, "Answer structure must be one of: nested, flat, basic\n");
    return NULL;
}

bool parse_app_config(int argc, char **argv, AppConfig *config) {
    config->file_path = NULL;
    config->output_path = NULL;
    config->pretty_print = 0;
    config->answer_structure = NULL;

#if DEBUG
    if (argc == 1) {
        config->file_path = "test_forms/basic-group-form.json";
        config->pretty_print = 4;
        return true;
    }
#endif

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (strcmp(arg, "-o") == 0 || strcmp(arg, "--output") == 0) {
            if (++i >= argc || argv[i][0] == '\0') {
                fprintf(stderr, "Missing value for %s\n", arg);
                return false;
            }
            config->output_path = argv[i];
            continue;
        }

        if (strcmp(arg, "-p") == 0 || strcmp(arg, "--pretty-print") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Missing value for %s\n", arg);
                return false;
            }
            if (!parse_pretty_print(argv[i], &config->pretty_print)) return false;
            continue;
        }

        if (strcmp(arg, "-a") == 0 || strcmp(arg, "--answer-structure") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Missing value for %s\n", arg);
                return false;
            }
            config->answer_structure = parse_answer_structure(argv[i]);
            if (config->answer_structure == NULL) return false;
            continue;
        }

        if (config->file_path == NULL) {
            config->file_path = arg;
            continue;
        }

        fprintf(stderr, "Unexpected argument: %s\n", arg);
        return false;
    }

    if (config->file_path == NULL) {
        fprintf(stderr, "Usage: %s <input-file> [-o|--output <output-file>] [-p|--pretty-print <indent>] [-a|--answer-structure <nested|flat|basic>]\n", argv[0]);
        return false;
    }

    return true;
}
