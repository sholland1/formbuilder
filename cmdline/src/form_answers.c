#include "form_cli.h"

#include <string.h>

void append_raw_answer(Answers *answers, const char *id, const char *value) {
    Answer a = {
        .id = strdup(id),
        .type = ft_text,
        .value = strdup(value),
    };
    nob_da_append(answers, a);
}

void append_static_answer(Answers *answers, const char *id, const char *value) {
    Answer a = {
        .id = strdup(id),
        .type = ft_text,
        .value = value,
    };
    nob_da_append(answers, a);
}

void append_null_answer(Answers *answers, const char *id) {
    append_static_answer(answers, id, "null");
}

void append_quoted_answer(Answers *answers, const char *id, const char *value) {
    static char quoted_answer_buffer[BUFFER_LEN + 2];
    sprintf(quoted_answer_buffer, "\"%s\"", value);
    append_raw_answer(answers, id, quoted_answer_buffer);
}

void append_multiselect_answer(Answers *answers, const char *id, SelectOptions *opts) {
    Answer a = {
        .id = strdup(id),
        .type = ft_multiselect,
    };
    a.options.capacity = 0;
    a.options.count = 0;
    a.options.items = NULL;
    nob_da_append_many(&a.options, opts->items, opts->count);
    nob_da_append(answers, a);
}

bool is_empty(const char *s) {
    return !s || !*s;
}
