#include "form_app.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

static bool parse_app_config(int argc, char **argv, AppConfig *config) {
    if (argc < 2 || argc > 3) {
    #if DEBUG
        config->file_path = "test.json";
        config->pretty_print = 4;
        return true;
    #else
        fprintf(stderr, "Usage: %s <input-file> [pretty-print-indent]\n", argv[0]);
        return false;
    #endif
    }

    config->file_path = argv[1];
    config->pretty_print = 0;

    if (argc == 3) {
        char *end = NULL;
        long value = strtol(argv[2], &end, 10);
        if (argv[2][0] == '\0' || end == NULL || *end != '\0' || errno == ERANGE || value < 0 || value > INT_MAX) {
            fprintf(stderr, "Pretty print indent must be an integer number of spaces between 0 and %d\n", INT_MAX);
            return false;
        }
        config->pretty_print = (int) value;
    }

    return true;
}

int main(int argc, char **argv) {
    AppConfig config = {0};
    if (!parse_app_config(argc, argv, &config)) return 1;

    Form form = {0};
    if (!load_form_from_file(config.file_path, &form)) {
        fprintf(stderr, "Failed to parse form in %s\n", config.file_path);
        return 1;
    }

    terminal_init();

    Answers answers = {0};
    nob_da_reserve(&answers, form.fields.count);

    display_form(&form, &answers);

    terminal_deinit();

    output_answers(&answers, config.pretty_print, stdout);

    return 0;
}
