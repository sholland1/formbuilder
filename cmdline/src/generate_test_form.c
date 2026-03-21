#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define JIM_IMPLEMENTATION
#define NOB_IMPLEMENTATION
#include "jim.h"
#include "nob.h"

#define DEFAULT_OUTPUT_PATH "comprehensive_test_form.json"

typedef enum {
    rs_missing,
    rs_true,
    rs_false,
} RequiredState;

static const char *required_state_label(RequiredState state) {
    switch (state) {
    case rs_missing: return "missing";
    case rs_true: return "True";
    case rs_false: return "False";
    default: NOB_UNREACHABLE("invalid required state");
    }
}

static const char *yesno(bool value) {
    return value ? "yes" : "no";
}

static void write_field_header(Jim *jim, const char *id, const char *type, const char *question) {
    jim_object_begin(jim);
    jim_member_key(jim, "id");
    jim_string(jim, id);
    jim_member_key(jim, "type");
    jim_string(jim, type);
    if (question != NULL) {
        jim_member_key(jim, "question");
        jim_string(jim, question);
    }
}

static void write_required_member(Jim *jim, RequiredState state) {
    if (state == rs_missing) return;
    jim_member_key(jim, "required");
    jim_bool(jim, state == rs_true);
}

static void write_string_array_member(Jim *jim, const char *key, const char **items, size_t count) {
    jim_member_key(jim, key);
    jim_array_begin(jim);
    for (size_t i = 0; i < count; ++i) {
        jim_string(jim, items[i]);
    }
    jim_array_end(jim);
}

static void emit_text_fields(Jim *jim) {
    RequiredState required_states[] = {rs_missing, rs_true, rs_false};
    for (size_t req = 0; req < NOB_ARRAY_LEN(required_states); ++req) {
        for (int placeholder = 0; placeholder <= 1; ++placeholder) {
            for (int maxlength = 0; maxlength <= 1; ++maxlength) {
                for (int pattern = 0; pattern <= 1; ++pattern) {
                    char id[256];
                    char question[256];
                    snprintf(id, sizeof(id), "text_required-%s_placeholder-%s_maxlength-%s_pattern-%s",
                        required_state_label(required_states[req]),
                        yesno(placeholder),
                        yesno(maxlength),
                        yesno(pattern));
                    snprintf(question, sizeof(question), "Text field: required-%s, placeholder-%s, maxlength-%s, pattern-%s",
                        required_state_label(required_states[req]),
                        yesno(placeholder),
                        yesno(maxlength),
                        yesno(pattern));

                    write_field_header(jim, id, "text", question);
                    write_required_member(jim, required_states[req]);
                    if (placeholder) {
                        jim_member_key(jim, "placeholder");
                        jim_string(jim, "(text placeholder)");
                    }
                    if (maxlength) {
                        jim_member_key(jim, "maxlength");
                        jim_integer(jim, 16);
                    }
                    if (pattern) {
                        jim_member_key(jim, "pattern");
                        jim_string(jim, "^[A-Z]{2}[0-9]{2}$");
                    }
                    jim_object_end(jim);
                }
            }
        }
    }
}

static void emit_multitext_fields(Jim *jim) {
    RequiredState required_states[] = {rs_missing, rs_true, rs_false};
    for (size_t req = 0; req < NOB_ARRAY_LEN(required_states); ++req) {
        for (int placeholder = 0; placeholder <= 1; ++placeholder) {
            for (int minp = 0; minp <= 1; ++minp) {
                for (int maxp = 0; maxp <= 1; ++maxp) {
                    for (int maxlength = 0; maxlength <= 1; ++maxlength) {
                        for (int pattern = 0; pattern <= 1; ++pattern) {
                            char id[320];
                            char question[320];
                            snprintf(id, sizeof(id),
                                "multitext_required-%s_placeholder-%s_min-%s_max-%s_maxlength-%s_pattern-%s",
                                required_state_label(required_states[req]),
                                yesno(placeholder),
                                yesno(minp),
                                yesno(maxp),
                                yesno(maxlength),
                                yesno(pattern));
                            snprintf(question, sizeof(question),
                                "Multitext field: required-%s, placeholder-%s, min-%s, max-%s, maxlength-%s, pattern-%s",
                                required_state_label(required_states[req]),
                                yesno(placeholder),
                                yesno(minp),
                                yesno(maxp),
                                yesno(maxlength),
                                yesno(pattern));

                            write_field_header(jim, id, "multitext", question);
                            write_required_member(jim, required_states[req]);
                            if (placeholder) {
                                jim_member_key(jim, "placeholder");
                                jim_string(jim, "(comma separated values)");
                            }
                            if (minp) {
                                jim_member_key(jim, "min");
                                jim_integer(jim, 1);
                            }
                            if (maxp) {
                                jim_member_key(jim, "max");
                                jim_integer(jim, 4);
                            }
                            if (maxlength) {
                                jim_member_key(jim, "maxlength");
                                jim_integer(jim, 12);
                            }
                            if (pattern) {
                                jim_member_key(jim, "pattern");
                                jim_string(jim, "^[a-z]+$");
                            }
                            jim_object_end(jim);
                        }
                    }
                }
            }
        }
    }
}

static void emit_number_fields(Jim *jim) {
    RequiredState required_states[] = {rs_missing, rs_true, rs_false};
    for (size_t req = 0; req < NOB_ARRAY_LEN(required_states); ++req) {
        for (int minp = 0; minp <= 1; ++minp) {
            for (int maxp = 0; maxp <= 1; ++maxp) {
                for (int step = 0; step <= 1; ++step) {
                    char id[256];
                    char question[256];
                    snprintf(id, sizeof(id), "number_required-%s_min-%s_max-%s_step-%s",
                        required_state_label(required_states[req]),
                        yesno(minp),
                        yesno(maxp),
                        yesno(step));
                    snprintf(question, sizeof(question), "Number field: required-%s, min-%s, max-%s, step-%s",
                        required_state_label(required_states[req]),
                        yesno(minp),
                        yesno(maxp),
                        yesno(step));

                    write_field_header(jim, id, "number", question);
                    write_required_member(jim, required_states[req]);
                    if (minp) {
                        jim_member_key(jim, "min");
                        jim_integer(jim, 0);
                    }
                    if (maxp) {
                        jim_member_key(jim, "max");
                        jim_integer(jim, 100);
                    }
                    if (step) {
                        jim_member_key(jim, "step");
                        jim_double(jim, 0.5, -1);
                    }
                    jim_object_end(jim);
                }
            }
        }
    }
}

static void emit_select_fields(Jim *jim) {
    static const char *options[] = {"alpha", "beta", "gamma"};
    RequiredState required_states[] = {rs_missing, rs_true, rs_false};
    for (size_t req = 0; req < NOB_ARRAY_LEN(required_states); ++req) {
        char label[64];
        char id[96];
        char question[96];
        snprintf(label, sizeof(label), "required-%s", required_state_label(required_states[req]));
        snprintf(id, sizeof(id), "select_%s", label);
        snprintf(question, sizeof(question), "Select field: %s", label);

        write_field_header(jim, id, "select", question);
        write_string_array_member(jim, "options", options, NOB_ARRAY_LEN(options));
        write_required_member(jim, required_states[req]);
        jim_object_end(jim);
    }
}

static void emit_multiselect_fields(Jim *jim) {
    static const char *options[] = {"red", "green", "blue", "orange"};
    for (int minp = 0; minp <= 1; ++minp) {
        for (int maxp = 0; maxp <= 1; ++maxp) {
            char id[128];
            char question[128];
            snprintf(id, sizeof(id), "multiselect_min-%s_max-%s", yesno(minp), yesno(maxp));
            snprintf(question, sizeof(question), "Multiselect field: min-%s, max-%s", yesno(minp), yesno(maxp));

            write_field_header(jim, id, "multiselect", question);
            write_string_array_member(jim, "options", options, NOB_ARRAY_LEN(options));
            if (minp) {
                jim_member_key(jim, "min");
                jim_integer(jim, 1);
            }
            if (maxp) {
                jim_member_key(jim, "max");
                jim_integer(jim, 3);
            }
            jim_object_end(jim);
        }
    }
}

static void emit_timestamp_fields(Jim *jim) {
    write_field_header(jim, "timestamp_basic", "timestamp", NULL);
    jim_object_end(jim);
}

static void write_optional_date_member(Jim *jim, const char *key, const char *value) {
    if (strcmp(value, "missing") == 0) return;
    jim_member_key(jim, key);
    jim_string(jim, value);
}

static void emit_date_fields(Jim *jim) {
    static const char *date_values[] = {"1900-01-01", "[today]", "2026-03-19", "2099-12-31", "missing"};
    RequiredState required_states[] = {rs_missing, rs_true, rs_false};
    for (size_t req = 0; req < NOB_ARRAY_LEN(required_states); ++req) {
        for (size_t start = 0; start < NOB_ARRAY_LEN(date_values); ++start) {
            for (size_t end = 0; end < NOB_ARRAY_LEN(date_values); ++end) {
                char id[320];
                char question[320];
                snprintf(id, sizeof(id), "date_required-%s_start-date-%s_end-date-%s",
                    required_state_label(required_states[req]),
                    date_values[start],
                    date_values[end]);
                snprintf(question, sizeof(question), "Date field: required-%s, start-date-%s, end-date-%s",
                    required_state_label(required_states[req]),
                    date_values[start],
                    date_values[end]);

                write_field_header(jim, id, "date", question);
                write_required_member(jim, required_states[req]);
                write_optional_date_member(jim, "start_date", date_values[start]);
                write_optional_date_member(jim, "end_date", date_values[end]);
                jim_object_end(jim);
            }
        }
    }
}

static void emit_counter_fields(Jim *jim) {
    write_field_header(jim, "counter_basic", "counter", "Counter field");
    jim_object_end(jim);
}

static void emit_color_fields(Jim *jim) {
    write_field_header(jim, "color_basic", "color", "Color field");
    jim_object_end(jim);
}

static void emit_bool_fields(Jim *jim) {
    RequiredState required_states[] = {rs_missing, rs_true, rs_false};
    for (size_t req = 0; req < NOB_ARRAY_LEN(required_states); ++req) {
        char label[64];
        char id[96];
        char question[96];
        snprintf(label, sizeof(label), "required-%s", required_state_label(required_states[req]));
        snprintf(id, sizeof(id), "bool_%s", label);
        snprintf(question, sizeof(question), "Boolean field: %s", label);

        write_field_header(jim, id, "bool", question);
        write_required_member(jim, required_states[req]);
        jim_object_end(jim);
    }
}

static void emit_timer_fields(Jim *jim) {
    write_field_header(jim, "timer_basic", "timer", "Timer field");
    jim_object_end(jim);
}

int main(int argc, char **argv) {
    const char *output_path = argc > 1 ? argv[1] : DEFAULT_OUTPUT_PATH;

    Jim jim = {.pp = 4};
    jim_object_begin(&jim);
    jim_member_key(&jim, "id");
    jim_string(&jim, "basic-form");
    jim_member_key(&jim, "title");
    jim_string(&jim, "All Field Permutations");
    jim_member_key(&jim, "fields");
    jim_array_begin(&jim);
    emit_text_fields(&jim);
    emit_multitext_fields(&jim);
    emit_number_fields(&jim);
    emit_select_fields(&jim);
    emit_multiselect_fields(&jim);
    emit_timestamp_fields(&jim);
    emit_date_fields(&jim);
    emit_counter_fields(&jim);
    emit_color_fields(&jim);
    emit_bool_fields(&jim);
    emit_timer_fields(&jim);
    jim_array_end(&jim);
    jim_object_end(&jim);

    FILE *output = fopen(output_path, "w");
    if (output == NULL) {
        perror(output_path);
        return 1;
    }

    fwrite(jim.sink, jim.sink_count, 1, output);
    fputc('\n', output);
    fclose(output);

    printf("Wrote %zu bytes to %s\n", jim.sink_count + 1, output_path);
    return 0;
}
