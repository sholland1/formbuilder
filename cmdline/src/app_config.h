#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "types.h"

typedef struct {
    const char *file_path;
    const char *output_path;
    int pretty_print;
    AnswerStructureType *answer_structure;
} AppConfig;

bool parse_app_config(int argc, char **argv, AppConfig *config);

#endif
