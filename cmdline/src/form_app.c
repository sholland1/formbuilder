#define JIM_IMPLEMENTATION
#define JIMP_IMPLEMENTATION
#define NOB_IMPLEMENTATION
#define NOB_UNSTRIP_PREFIX
#include "jim_form.h"
#include "form_app.h"

#include <locale.h>
#include <uuid/uuid.h>

bool load_form_from_file(const char *file_path, Form *form) {
    Nob_String_Builder sb = {0};
    if (!nob_read_entire_file(file_path, &sb)) return false;

    Jimp jimp = {0};
    jimp_begin(&jimp, file_path, sb.items, sb.count);
    return jimp_form(&jimp, form);
}

void warn_unimplemented_field_type(Field *f) {
    const char *type_name = NULL;
    switch (f->type) {
#define X(name) case ft_##name: type_name = #name; break;
        UNIMPLEMENTED_FIELDTYPES
#undef X
        default: return;
    }
    fprintf(tty_out, "Skipping field '%s' because the '%s' field type is unimplemented.\r\n", f->id, type_name);
}

void fprint_depth(FILE *stream, int depth) {
    for (int i = 0; i < depth; i++) {
        fprintf(stream, "│");
    }
}

typedef struct {
    char answer_buffer[ANSWER_BUFFER_LEN];
    const char *timestamp_id;
    const char *guid_id;
    SelectOptions opts;
} SpecialFields;

void display_field(const Field *f, int depth, SpecialFields *sp, Answers *answers) {
    sp->answer_buffer[0] = '\0';
    sp->opts.count = 0;

    ASSERT_FIELD_TYPES_LENGTH(16);

    switch (f->type) {
    case ft_text:
        read_text(f, depth, sp->answer_buffer);
        if (is_empty(sp->answer_buffer)) {
            append_null_answer(answers, f->id);
        }
        else {
            append_quoted_answer(answers, f->id, sp->answer_buffer);
        }
        break;

    case ft_number:
        read_number(f, depth, sp->answer_buffer);
        if (is_empty(sp->answer_buffer)) {
            append_null_answer(answers, f->id);
        }
        else {
            append_raw_answer(answers, f->id, sp->answer_buffer);
        }
        break;

    case ft_select:
        read_select(f, depth, sp->answer_buffer);
        if (is_empty(sp->answer_buffer)) {
            append_null_answer(answers, f->id);
        }
        else {
            append_quoted_answer(answers, f->id, sp->answer_buffer);
        }
        break;

    case ft_multiselect:
        read_multiselect(f, depth, &sp->opts);
        append_multiselect_answer(answers, f->id, &sp->opts);
        break;

    case ft_multitext:
        read_multitext(f, depth, sp->answer_buffer);
        append_multitext_answer(answers, f->id, sp->answer_buffer);
        break;

    case ft_date: {
        struct tm d;
        if (read_date(f, depth, &d)) {
            strftime(sp->answer_buffer, ANSWER_BUFFER_LEN, "%Y-%m-%d", &d);
            append_quoted_answer(answers, f->id, sp->answer_buffer);
        }
        else {
            append_null_answer(answers, f->id);
        }
    } break;

    case ft_counter:
        snprintf(sp->answer_buffer, ANSWER_BUFFER_LEN, "%lld", read_counter(f, depth));
        append_raw_answer(answers, f->id, sp->answer_buffer);
        break;

    case ft_color:
        color_to_str(sp->answer_buffer, read_color(f, depth));
        append_quoted_answer(answers, f->id, sp->answer_buffer);
        break;

    case ft_bool: {
        Tristate choice = read_bool(f, depth);
        switch (choice) {
        case ts_null: append_null_answer(answers, f->id); break;
        case ts_true: append_static_answer(answers, f->id, "true"); break;
        case ts_false: append_static_answer(answers, f->id, "false"); break;
        }
    } break;

    case ft_timer: {
        uint64_t duration_in_nanoseconds = read_timer(f, depth);
        ns_to_iso8601_duration(duration_in_nanoseconds, sp->answer_buffer, ANSWER_BUFFER_LEN);
        append_quoted_answer(answers, f->id, sp->answer_buffer);
    } break;

    case ft_timestamp:
        sp->timestamp_id = f->id;
        break;

    case ft_guid:
        sp->guid_id = f->id;
        break;

    case ft_file:
    case ft_signature:
        break;

    case ft_rating: {
        Rating r = read_rating(f, depth);
        sprint_score(sp->answer_buffer, r);
        append_quoted_answer(answers, f->id, sp->answer_buffer);
    } break;

    case ft_group: {
        fprint_depth(tty_out, depth);
        fprintf(tty_out, "┌\r\n");

        fprint_depth(tty_out, depth+1);
        fprintf(tty_out, ITALIC" %s"RESET"\r\n", f->group.label);

        fprint_depth(tty_out, depth);
        fprintf(tty_out, "├\r\n");

        Answers *nested_answers = (Answers*)calloc(1, sizeof(Answers));
        nob_da_reserve(nested_answers, f->group.fields->count);
        nob_da_foreach(Field, ff, f->group.fields) {
            display_field(ff, depth+1, sp, nested_answers);
        }
        append_group_answers(answers, f->id, nested_answers);

        fprint_depth(tty_out, depth);
        fprintf(tty_out, "└\r\n");
        fflush(tty_out);
    } break;

    default:
        NOB_UNREACHABLE("Unidentified type!");
    }
}

void warn_unimplemented_fields(const Fields *fields) {
    nob_da_foreach(Field, f, fields) {
        warn_unimplemented_field_type(f);
        if (f->type == ft_group) {
            warn_unimplemented_fields(f->group.fields);
        }
    }
}

void display_form(const Form *form, Answers *answers) {
    fprintf(tty_out, CLR HOME BOLD"%s"RESET"\r\n", form->title);

    //TODO: implement these field types
    warn_unimplemented_fields(&form->fields);

    SpecialFields sp = {0};
    nob_da_foreach(Field, f, &form->fields) {
        display_field(f, 0, &sp, answers);
    }

    if (sp.timestamp_id) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        strftime(sp.answer_buffer, ANSWER_BUFFER_LEN, "%Y-%m-%d %H:%M:%S", t);
        append_quoted_answer(answers, sp.timestamp_id, sp.answer_buffer);
    }

    if (sp.guid_id) {
        uuid_t uuid;
        uuid_generate(uuid);
        uuid_unparse_lower(uuid, sp.answer_buffer);
        append_quoted_answer(answers, sp.guid_id, sp.answer_buffer);
    }
}

void output_answers(const Answers *answers, int pretty_print, AnswerStructureType answer_structure_type, FILE *stream) {
    setlocale(LC_NUMERIC, "C");

    Jim jim = {.pp = pretty_print};
    jim_answers(&jim, answers, answer_structure_type);
    fwrite(jim.sink, jim.sink_count, 1, stream);
}

void output_form(const Form *form, int pretty_print, FILE *stream) {
    setlocale(LC_NUMERIC, "C");

    Jim jim = {.pp = pretty_print};
    jim_form(&jim, form);
    fwrite(jim.sink, jim.sink_count, 1, stream);
}
