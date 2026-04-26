#include "form_app.h"
#include "app_config.h"

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

    AnswerStructureType answer_structure_type =
        config.answer_structure != NULL ? *config.answer_structure
        : form.answer_structure != NULL ? *form.answer_structure
        : ast_nested;

    FILE *output_stream = stdout;
    if (config.output_path != NULL) {
        output_stream = fopen(config.output_path, "w");
        if (output_stream == NULL) {
            fprintf(stderr, "Failed to open output file %s\n", config.output_path);
            return 1;
        }
    }

    output_answers(&answers, config.pretty_print, answer_structure_type, output_stream);

    if (output_stream != stdout) {
        fclose(output_stream);
    }

    return 0;
}
