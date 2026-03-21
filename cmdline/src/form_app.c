#define JIM_IMPLEMENTATION
#define JIMP_IMPLEMENTATION
#define NOB_IMPLEMENTATION
#define NOB_UNSTRIP_PREFIX
#include "jim_form.h"
#include "form_app.h"

#include <locale.h>

bool load_form_from_file(const char *file_path, Form *form) {
    Nob_String_Builder sb = {0};
    if (!nob_read_entire_file(file_path, &sb)) return false;

    Jimp jimp = {0};
    jimp_begin(&jimp, file_path, sb.items, sb.count);
    return jimp_form(&jimp, form);
}

void display_form(const Form *form, Answers *answers) {
    fprintf(tty_out, CLR HOME BOLD"%s"RESET"\r\n", form->title);

    static char answer_buffer[BUFFER_LEN];
    const char *timestamp_field_id = NULL;
    SelectOptions opts = {0};
    nob_da_foreach(Field, f, &form->fields) {
        answer_buffer[0] = '\0';
        opts.count = 0;

        switch (f->type) {
        case ft_text:
            read_text(f, answer_buffer);
            if (is_empty(answer_buffer)) {
                append_null_answer(answers, f->id);
            }
            else {
                append_quoted_answer(answers, f->id, answer_buffer);
            }
            break;

        case ft_number:
            read_number(f, answer_buffer);
            if (is_empty(answer_buffer)) {
                append_null_answer(answers, f->id);
            }
            else {
                append_raw_answer(answers, f->id, answer_buffer);
            }
            break;

        case ft_select:
            read_select(f, answer_buffer);
            if (is_empty(answer_buffer)) {
                append_null_answer(answers, f->id);
            }
            else {
                append_quoted_answer(answers, f->id, answer_buffer);
            }
            break;

        case ft_multiselect:
            read_multiselect(f, &opts);
            append_multiselect_answer(answers, f->id, &opts);
            break;

        case ft_multitext:
            read_multitext(f, answer_buffer);
            append_multitext_answer(answers, f->id, answer_buffer);
            break;

        case ft_date: {
            struct tm d;
            if (read_date(f, &d)) {
                strftime(answer_buffer, sizeof(answer_buffer), "%Y-%m-%d", &d);
                append_quoted_answer(answers, f->id, answer_buffer);
            }
            else {
                append_null_answer(answers, f->id);
            }
        } break;

        case ft_counter:
            sprintf(answer_buffer, "%lu", read_counter(f));
            append_raw_answer(answers, f->id, answer_buffer);
            break;

        case ft_color:
            color_to_str(answer_buffer, read_color(f));
            append_quoted_answer(answers, f->id, answer_buffer);
            break;

        case ft_bool: {
            Tristate choice = read_bool(f);
            switch (choice) {
            case ts_null: append_null_answer(answers, f->id); break;
            case ts_true: append_static_answer(answers, f->id, "true"); break;
            case ts_false: append_static_answer(answers, f->id, "false"); break;
            }
        } break;

        case ft_timer: {
            uint64_t duration_in_nanoseconds = read_timer(f);
            ns_to_iso8601_duration(duration_in_nanoseconds, answer_buffer, BUFFER_LEN);
            append_quoted_answer(answers, f->id, answer_buffer);
        } break;

        case ft_timestamp:
            timestamp_field_id = f->id;
            break;

        default:
            NOB_UNREACHABLE("Unidentified type!");
        }
    }

    if (timestamp_field_id) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        strftime(answer_buffer, sizeof(answer_buffer), "%Y-%m-%d %H:%M:%S", t);
        append_quoted_answer(answers, timestamp_field_id, answer_buffer);
    }
}

void output_answers(const Answers *answers, int pretty_print, FILE *stream) {
    setlocale(LC_NUMERIC, "C");

    Jim jim = {.pp = pretty_print};
    jim_answers(&jim, answers);
    fwrite(jim.sink, jim.sink_count, 1, stream);
}

void output_form(const Form *form, int pretty_print, FILE *stream) {
    setlocale(LC_NUMERIC, "C");

    Jim jim = {.pp = pretty_print};
    jim_form(&jim, form);
    fwrite(jim.sink, jim.sink_count, 1, stream);
}
