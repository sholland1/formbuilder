#include "jimp.h"
#include "jim.h"
#include "libregexp/libregexp.h"

#include "regex.h"
#include "types.h"

#include <math.h>
#include <time.h>

#define REAL_YEAR(year) (NOB_ASSERT((year) < 1900), (year)+1900)
#define FAKE_YEAR(year) (NOB_ASSERT((year) >= 1900), (year)-1900)

#define DATE_BUFFER_LEN 11

void jim_field_type(Jim *jim, FieldType t) {
    switch (t) {
#define X(name) case ft_##name: jim_string(jim, #name); break;
        FIELDTYPES
#undef X
        default: NOB_UNREACHABLE("Unidentified type!");
    }
}

// TODO: maybe match with regex
FieldType parse_field_type(const char *type_str) {
#define X(name) if (strcmp(type_str, #name) == 0) return ft_##name;
    FIELDTYPES
#undef X
    NOB_UNREACHABLE("Unidentified type!");
}

void jim_answer_structure_type(Jim *jim, AnswerStructureType t) {
    switch (t) {
    case ast_nested: jim_string(jim, "nested"); break;
    case ast_flat: jim_string(jim, "flat"); break;
    case ast_basic: jim_string(jim, "basic"); break;
    default: NOB_UNREACHABLE("Unidentified type!");
    }
}

static AnswerStructureType *parse_answer_structure_type(const char *type_str) {
    if (strcmp(type_str, "basic") == 0) return ANSWER_STRUCTURE_BASIC;
    if (strcmp(type_str, "flat") == 0) return ANSWER_STRUCTURE_FLAT;
    if (strcmp(type_str, "nested") == 0) return ANSWER_STRUCTURE_NESTED;
    NOB_UNREACHABLE("Unidentified type!");
}

bool parse_yyyy_mm_dd(const char *str, int *real_year, int *month, int *day) {
    if (sscanf(str, "%d-%d-%d", real_year, month, day) == 3) {
        // TODO: Better validation
        if (*real_year >= 1 && *real_year <= 9999 &&
            *month     >= 1 && *month     <= 12 &&
            *day       >= 1 && *day       <= 31) {
            return true;
        }
    }
    return false;
}

bool parse_to_tm(const char *str, struct tm* tm) {
    int real_year, m, d;
    if (!parse_yyyy_mm_dd(str, &real_year, &m, &d)) return false;
    tm->tm_year = FAKE_YEAR(real_year);
    tm->tm_mon  = m - 1;
    tm->tm_mday = d;
    tm->tm_hour = 12;
    return true;
}

static void jim_field(Jim *jim, const Field *x) {
    jim_object_begin(jim);
    jim_member_key(jim, "id");
    jim_string(jim, x->id);
    jim_member_key(jim, "type");
    jim_field_type(jim, x->type);

    ASSERT_FIELD_TYPES_LENGTH(16);

    switch (x->type) {
        case ft_text: {
            TextFieldMembers p = x->text;
            jim_member_key(jim, "label"); jim_string(jim, p.label);
            if (!p.required) {jim_member_key(jim, "required"); jim_bool(jim, p.required);}
            if (p.placeholder) { jim_member_key(jim, "placeholder"); jim_string(jim, p.placeholder);}
            if (p.maxlength != SIZE_MAX) { jim_member_key(jim, "maxlength"); jim_integer(jim, p.maxlength);}
            if (p.pattern) {jim_member_key(jim, "pattern"); jim_string(jim, p.pattern);}
        } break;

        case ft_number: {
            NumberFieldMembers p = x->number;
            jim_member_key(jim, "label"); jim_string(jim, p.label);
            if (!p.required) {jim_member_key(jim, "required"); jim_bool(jim, p.required);}
            const int precision = -1;
            if (!isnan(p.min)) {jim_member_key(jim, "min"); jim_double(jim, p.min, precision);}
            if (!isnan(p.max)) {jim_member_key(jim, "max"); jim_double(jim, p.max, precision);}
            if (p.step != 1) {jim_member_key(jim, "step"); jim_double(jim, p.step, precision);}
        } break;

        case ft_select: {
            SelectFieldMembers p = x->select;
            jim_member_key(jim, "label"); jim_string(jim, p.label);
            if (!p.required) {jim_member_key(jim, "required"); jim_bool(jim, p.required);}
            jim_member_key(jim, "options");
            jim_array_begin(jim);
            nob_da_foreach(char *, x, &p.options) {
                jim_string(jim, *x);
            }
            jim_array_end(jim);
        } break;

        case ft_multiselect: {
            MultiSelectFieldMembers p = x->multiselect;
            jim_member_key(jim, "label"); jim_string(jim, p.label);
            jim_member_key(jim, "options");
            jim_array_begin(jim);
            nob_da_foreach(char *, x, &p.options) {
                jim_string(jim, *x);
            }
            jim_array_end(jim);
            if (p.min != 0) {jim_member_key(jim, "min"); jim_integer(jim, p.min);}
            if (p.max != INT_MAX) {jim_member_key(jim, "max"); jim_integer(jim, p.max);}
        } break;

        case ft_multitext: {
            MultiTextFieldMembers p = x->multitext;
            jim_member_key(jim, "label"); jim_string(jim, p.label);
            if (!p.required) {jim_member_key(jim, "required"); jim_bool(jim, p.required);}
            if (p.placeholder) { jim_member_key(jim, "placeholder"); jim_string(jim, p.placeholder);}
            if (p.min != 0) {jim_member_key(jim, "min"); jim_integer(jim, p.min);}
            if (p.max != INT_MAX) {jim_member_key(jim, "max"); jim_integer(jim, p.max);}
            if (p.maxlength != SIZE_MAX) { jim_member_key(jim, "maxlength"); jim_integer(jim, p.maxlength);}
            if (p.pattern) {jim_member_key(jim, "pattern"); jim_string(jim, p.pattern);}
        } break;

        case ft_date: {
            static char date_buffer[DATE_BUFFER_LEN];
            DateFieldMembers p = x->date;
            jim_member_key(jim, "label"); jim_string(jim, p.label);
            if (!p.required) {jim_member_key(jim, "required"); jim_bool(jim, p.required);}
            if (p.start_date.is_today) {
                jim_member_key(jim, "start_date");
                jim_string(jim, "[today]");
            }
            else if (p.start_date.dt) {
                jim_member_key(jim, "start_date");
                strftime(date_buffer, DATE_BUFFER_LEN, "%Y-%m-%d", p.start_date.dt);
                jim_string(jim, date_buffer);
            }
            if (p.end_date.is_today) {
                jim_member_key(jim, "end_date");
                jim_string(jim, "[today]");
            }
            else if (p.end_date.dt) {
                jim_member_key(jim, "end_date");
                strftime(date_buffer, DATE_BUFFER_LEN, "%Y-%m-%d", p.end_date.dt);
                jim_string(jim, date_buffer);
            }
        } break;

        case ft_counter:
            jim_member_key(jim, "label"); jim_string(jim, x->counter.label);
            break;

        case ft_color:
            jim_member_key(jim, "label"); jim_string(jim, x->color.label);
            break;

        case ft_bool: {
            RequiredQFieldMembers p = x->boolean;
            jim_member_key(jim, "label"); jim_string(jim, p.label);
            if (!p.required) {jim_member_key(jim, "required"); jim_bool(jim, p.required);}
        } break;

        case ft_timer:
            jim_member_key(jim, "label"); jim_string(jim, x->timer.label);
            break;

        case ft_timestamp:
        case ft_guid:
            break;

        case ft_file: {
            FileFieldMembers p = x->file;
            jim_member_key(jim, "label"); jim_string(jim, p.label);
            jim_member_key(jim, "maxsize"); jim_integer(jim, p.maxsize);
            if (p.min != 0) {jim_member_key(jim, "min"); jim_integer(jim, p.min);}
            if (p.max != 1) {jim_member_key(jim, "max"); jim_integer(jim, p.max);}
            jim_member_key(jim, "fileexts");
            jim_array_begin(jim);
            nob_da_foreach(char *, x, &p.fileexts) {
                jim_string(jim, *x);
            }
            jim_array_end(jim);
        } break;

        case ft_signature: {
            RequiredQFieldMembers p = x->signature;
            jim_member_key(jim, "label"); jim_string(jim, p.label);
            if (!p.required) {jim_member_key(jim, "required"); jim_bool(jim, p.required);}
        } break;

        case ft_rating: {
            RatingFieldMembers p = x->rating;
            jim_member_key(jim, "label"); jim_string(jim, p.label);
            if (!p.required) {jim_member_key(jim, "required"); jim_bool(jim, p.required);}
            jim_member_key(jim, "maxrating"); jim_integer(jim, p.maxrating);
            const int precision = -1;
            jim_member_key(jim, "step"); jim_double(jim, p.step, precision);
        } break;

        case ft_group: {
            GroupFieldMembers p = x->group;
            jim_member_key(jim, "label"); jim_string(jim, p.label);

            jim_member_key(jim, "fields");
            jim_array_begin(jim);
            nob_da_foreach(Field, y, p.fields) {
                jim_field(jim, y);
            }
            jim_array_end(jim);
        } break;

        default: NOB_UNREACHABLE("Unidentified type!");
    }
    jim_object_end(jim);
}

void jim_form(Jim *jim, const Form *f) {
    jim_object_begin(jim);
    jim_member_key(jim, "id");
    jim_string(jim, f->id);
    jim_member_key(jim, "title");
    jim_string(jim, f->title);
    jim_member_key(jim, "answer_structure");
    jim_answer_structure_type(jim, f->answer_structure ? *f->answer_structure : ast_nested);

    jim_member_key(jim, "fields");
    jim_array_begin(jim);
    nob_da_foreach(Field, x, &f->fields) {
        jim_field(jim, x);
    }
    jim_array_end(jim);

    jim_object_end(jim);
}

void field_set_defaults(Field *field) {
    // Set non-zero defaults
    ASSERT_FIELD_TYPES_LENGTH(16);

    switch (field->type) {
        case ft_text:
            field->text.required = true;
            field->text.maxlength = SIZE_MAX;
            break;

        case ft_number:
            field->number.required = true;
            field->number.min = -NAN;
            field->number.max = NAN;
            field->number.step = 1.0f;
            break;

        case ft_select:
            field->select.required = true;
            break;

        case ft_multiselect:
            field->multiselect.max = INT_MAX;
            break;

        case ft_multitext:
            field->multitext.required = true;
            field->multitext.max = INT_MAX;
            field->multitext.maxlength = SIZE_MAX;
            break;

        case ft_date:
            field->date.required = true;
            field->date.start_date.dt = NULL;
            field->date.end_date.dt = NULL;
            break;

        case ft_bool:
            field->boolean.required = true;
            break;

        case ft_file:
            field->file.max = 1;
            break;

        case ft_signature:
            field->signature.required = true;
            break;

        case ft_rating:
            field->rating.required = true;
            field->rating.maxrating = mr_five;
            field->rating.step = 1.0f;
            break;

        case ft_group: {
            Fields *fields = (Fields*)calloc(1, sizeof(Fields));
            field->group.fields = fields;
        } break;

        case ft_counter:
        case ft_color:
        case ft_timer:
        case ft_timestamp:
        case ft_guid:
            break;

        default: NOB_UNREACHABLE("Unidentified type!");
    }
}

bool jimp_field(Jimp *jimp, Field *field) {
    if (!jimp_object_begin(jimp)) return false;
    while (jimp_object_member(jimp)) {
        if (strcmp(jimp->string, "id") == 0) {
            if (!jimp_string(jimp)) return false;
            field->id = strdup(jimp->string);
        }
        else if (strcmp(jimp->string, "type") == 0) {
            if (!jimp_string(jimp)) return false;
            field->type = parse_field_type(jimp->string);
            field_set_defaults(field);
        }
        else {
            ASSERT_FIELD_TYPES_LENGTH(16);

            switch (field->type) {
                case ft_text:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->text.label = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "required") == 0) {
                        if (!jimp_bool(jimp)) return false;
                        field->text.required = jimp->boolean;
                    }
                    else if (strcmp(jimp->string, "placeholder") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->text.placeholder = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "maxlength") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->text.maxlength = (size_t)jimp->number;
                    }
                    else if (strcmp(jimp->string, "pattern") == 0) {
                        if (!jimp_string(jimp)) return false;

                        field->text.regex = compile_regex(jimp->string, LRE_FLAG_DOTALL);
                        field->text.pattern = strdup(jimp->string);
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                case ft_number:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->number.label = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "required") == 0) {
                        if (!jimp_bool(jimp)) return false;
                        field->number.required = jimp->boolean;
                    }
                    else if (strcmp(jimp->string, "min") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->number.min = (double)jimp->number;
                    }
                    else if (strcmp(jimp->string, "max") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->number.max = (double)jimp->number;
                    }
                    else if (strcmp(jimp->string, "step") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->number.step = (double)jimp->number;
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                case ft_select:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->select.label = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "required") == 0) {
                        if (!jimp_bool(jimp)) return false;
                        field->select.required = jimp->boolean;
                    }
                    else if (strcmp(jimp->string, "options") == 0) {
                        if (!jimp_array_begin(jimp)) return false;
                        while (jimp_array_item(jimp)) {
                            if (!jimp_string(jimp)) return false;
                            nob_da_append(&field->select.options, strdup(jimp->string));
                        }
                        if (!jimp_array_end(jimp)) return false;
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                case ft_multiselect:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->multiselect.label = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "options") == 0) {
                        if (!jimp_array_begin(jimp)) return false;
                        while (jimp_array_item(jimp)) {
                            if (!jimp_string(jimp)) return false;
                            nob_da_append(&field->multiselect.options, strdup(jimp->string));
                        }
                        if (!jimp_array_end(jimp)) return false;
                    }
                    else if (strcmp(jimp->string, "min") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->multiselect.min = (int)jimp->number;
                    }
                    else if (strcmp(jimp->string, "max") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->multiselect.max = (int)jimp->number;
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                case ft_multitext:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->multitext.label = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "required") == 0) {
                        if (!jimp_bool(jimp)) return false;
                        field->multitext.required = jimp->boolean;
                    }
                    else if (strcmp(jimp->string, "placeholder") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->multitext.placeholder = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "min") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->multitext.min = (int)jimp->number;
                    }
                    else if (strcmp(jimp->string, "max") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->multitext.max = (int)jimp->number;
                    }
                    else if (strcmp(jimp->string, "maxlength") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->multitext.maxlength = (size_t)jimp->number;
                    }
                    else if (strcmp(jimp->string, "pattern") == 0) {
                        if (!jimp_string(jimp)) return false;

                        field->multitext.regex = compile_regex(jimp->string, LRE_FLAG_DOTALL);
                        field->multitext.pattern = strdup(jimp->string);
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                case ft_date:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->date.label = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "required") == 0) {
                        if (!jimp_bool(jimp)) return false;
                        field->date.required = jimp->boolean;
                    }
                    else if (strcmp(jimp->string, "start_date") == 0) {
                        if (jimp_is_null_ahead(jimp)) {
                            field->date.start_date.is_today = false;
                            field->date.start_date.dt = NULL;
                            break;
                        }
                        if (!jimp_string(jimp)) return false;
                        field->date.start_date.is_today = strcmp(jimp->string, "[today]") == 0;
                        if (!field->date.start_date.is_today) {
                            struct tm *dt = (struct tm*)malloc(sizeof(struct tm));
                            if (!parse_to_tm(jimp->string, dt)) return false;
                            field->date.start_date.dt = dt;
                        }
                    }
                    else if (strcmp(jimp->string, "end_date") == 0) {
                        if (jimp_is_null_ahead(jimp)) {
                            field->date.end_date.is_today = false;
                            field->date.end_date.dt = NULL;
                            break;
                        }
                        if (!jimp_string(jimp)) return false;
                        field->date.end_date.is_today = strcmp(jimp->string, "[today]") == 0;
                        if (!field->date.end_date.is_today) {
                            struct tm *dt = (struct tm*)malloc(sizeof(struct tm));
                            if (!parse_to_tm(jimp->string, dt)) return false;
                            field->date.end_date.dt = dt;
                        }
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                break;

                case ft_counter:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->counter.label = strdup(jimp->string);
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                case ft_color:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->color.label = strdup(jimp->string);
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                case ft_bool:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->boolean.label = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "required") == 0) {
                        if (!jimp_bool(jimp)) return false;
                        field->boolean.required = jimp->boolean;
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                case ft_timer:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->timer.label = strdup(jimp->string);
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                case ft_timestamp:
                case ft_guid:
                    jimp_unknown_member(jimp);
                    return false;
                    break;

                case ft_file:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->file.label = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "maxsize") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->file.maxsize = (size_t)jimp->number;
                    }
                    else if (strcmp(jimp->string, "min") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->file.min = (int)jimp->number;
                    }
                    else if (strcmp(jimp->string, "max") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->file.max = (int)jimp->number;
                    }
                    else if (strcmp(jimp->string, "fileexts") == 0) {
                        if (!jimp_array_begin(jimp)) return false;
                        while (jimp_array_item(jimp)) {
                            if (!jimp_string(jimp)) return false;
                            if (jimp->string[0] != '.') return false;
                            nob_da_append(&field->file.fileexts, strdup(jimp->string));
                        }
                        if (!jimp_array_end(jimp)) return false;
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                case ft_signature:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->signature.label = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "required") == 0) {
                        if (!jimp_bool(jimp)) return false;
                        field->signature.required = jimp->boolean;
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                case ft_rating:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->rating.label = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "required") == 0) {
                        if (!jimp_bool(jimp)) return false;
                        field->rating.required = jimp->boolean;
                    }
                    else if (strcmp(jimp->string, "maxrating") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->rating.maxrating = (MaxRating)jimp->number;
                    }
                    else if (strcmp(jimp->string, "step") == 0) {
                        if (!jimp_number(jimp)) return false;
                        field->rating.step = (double)jimp->number;
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                case ft_group:
                    if (strcmp(jimp->string, "label") == 0) {
                        if (!jimp_string(jimp)) return false;
                        field->group.label = strdup(jimp->string);
                    }
                    else if (strcmp(jimp->string, "fields") == 0) {
                        if (!jimp_array_begin(jimp)) return false;
                        while (jimp_array_item(jimp)) {
                            Field f = {0};
                            if (!jimp_field(jimp, &f)) return false;
                            nob_da_append(field->group.fields, f);
                        }
                        if (!jimp_array_end(jimp)) return false;
                    }
                    else {
                        jimp_unknown_member(jimp);
                        return false;
                    }
                    break;

                default: NOB_UNREACHABLE("Unidentified type!");
            }
        }
    }
    if (field->type == ft_number && field->number.max < field->number.min) return false;
    if (field->type == ft_multiselect && field->multiselect.max < field->multiselect.min) return false;
    if (field->type == ft_multitext && field->multitext.max < field->multitext.min) return false;
    if (field->type == ft_date) {
        DateFieldMembers d = field->date;
        if (!d.start_date.is_today && !d.end_date.is_today
            && !d.start_date.dt && !d.end_date.dt
            && d.end_date.dt < d.start_date.dt) return false;
    }
    if (field->type == ft_file && field->file.max < field->file.min) return false;
    if (field->type == ft_file && field->file.maxsize == 0) return false;
    return jimp_object_end(jimp);
}

bool jimp_form(Jimp *jimp, Form *form) {
    if (!jimp_object_begin(jimp)) return false;
    while (jimp_object_member(jimp)) {
        if (strcmp(jimp->string, "id") == 0) {
            if (!jimp_string(jimp)) return false;
            form->id = strdup(jimp->string);
        }
        else if (strcmp(jimp->string, "title") == 0) {
            if (!jimp_string(jimp)) return false;
            form->title = strdup(jimp->string);
        }
        else if (strcmp(jimp->string, "answer_structure") == 0) {
            if (!jimp_string(jimp)) return false;
            form->answer_structure = parse_answer_structure_type(jimp->string);
        }
        else if (strcmp(jimp->string, "fields") == 0) {
            if (!jimp_array_begin(jimp)) return false;
            while (jimp_array_item(jimp)) {
                Field f = {0};
                if (!jimp_field(jimp, &f)) return false;
                nob_da_append(&form->fields, f);
            }
            if (!jimp_array_end(jimp)) return false;
        }
        else {
            jimp_unknown_member(jimp);
            return false;
        }
    }
    return jimp_object_end(jimp);
}

typedef struct {
    size_t capacity;
    size_t count;
    const char **items;
} Nested_Ids;

const char *nob_da_join(Nested_Ids *ids, char delim) {
    static Nob_String_Builder sb = {0};
    sb.count = 0;
    nob_da_foreach(const char*, it, ids) {
        nob_sb_append_cstr(&sb, *it);
        nob_sb_append(&sb, delim);
    }
    sb.items[--sb.count] = '\0';
    return sb.items;
}

void print_answers_nested(const Answers *answers) {
    nob_da_foreach(Answer, a, answers) {
        printf("id: %s, type: %d\n", a->id, a->type);
        if (a->type == at_nested) {
            print_answers_nested(a->answers);
        }
    }
}

void jim_answers_flat_inner(Jim *jim, Nested_Ids *nested_ids, const Answers *answers) {
    nob_da_foreach(Answer, x, answers) {
        nob_da_append(nested_ids, x->id);
        const char *prefix = nob_da_join(nested_ids, '.');

        if (x->type == at_nested) {
            jim_answers_flat_inner(jim, nested_ids, x->answers);
        }
        else if (x->type == at_list) {
            jim_member_key(jim, prefix);
            jim_array_begin(jim);
            nob_da_foreach(char*, o, &x->options) {
                jim_string(jim, *o);
            }
            jim_array_end(jim);
        }
        else {
            jim_member_key(jim, prefix);
            jim_element_begin(jim);
            jim_write_cstr(jim, x->value);
            jim_element_end(jim);
        }
        nob_da_pop(nested_ids);
    }
}

void jim_answers_flat(Jim *jim, const Answers *answers) {
    // print_answers_nested(answers);
    jim_object_begin(jim);
    Nested_Ids nested_ids = {0};
    jim_answers_flat_inner(jim, &nested_ids, answers);
    jim_object_end(jim);
}

void jim_answers_basic_inner(Jim *jim, const Answers *answers) {
    nob_da_foreach(Answer, x, answers) {
        if (x->type == at_nested) {
            jim_answers_basic_inner(jim, x->answers);
        }
        else if (x->type == at_list) {
            jim_member_key(jim, x->id);
            jim_array_begin(jim);
            nob_da_foreach(char*, o, &x->options) {
                jim_string(jim, *o);
            }
            jim_array_end(jim);
        }
        else {
            jim_member_key(jim, x->id);
            jim_element_begin(jim);
            jim_write_cstr(jim, x->value);
            jim_element_end(jim);
        }
    }
}

void jim_answers_basic(Jim *jim, const Answers *answers) {
    jim_object_begin(jim);
    jim_answers_basic_inner(jim, answers);
    jim_object_end(jim);
}

void jim_answers_nested(Jim *jim, const Answers *answers) {
    jim_object_begin(jim);
    nob_da_foreach(Answer, x, answers) {
        jim_member_key(jim, x->id);
        if (x->type == at_nested) {
            jim_answers_nested(jim, x->answers);
        }
        else if (x->type == at_list) {
            jim_array_begin(jim);
            nob_da_foreach(char*, o, &x->options) {
                jim_string(jim, *o);
            }
            jim_array_end(jim);
        }
        else {
            jim_element_begin(jim);
            jim_write_cstr(jim, x->value);
            jim_element_end(jim);
        }
    }
    jim_object_end(jim);
}

void jim_answers(Jim *jim, const Answers *answers, AnswerStructureType answer_structure_type) {
    switch(answer_structure_type) {
    case ast_nested: jim_answers_nested(jim, answers); break;
    case ast_flat: jim_answers_flat(jim, answers); break;
    case ast_basic: jim_answers_basic(jim, answers); break;
    default: NOB_UNREACHABLE("Unidentified type!");
    }
}
