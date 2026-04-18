#ifndef TYPES_H
#define TYPES_H

#include "regex.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FIELDTYPES \
    X(text) \
    X(number) \
    X(select) \
    X(multiselect) \
    X(multitext) \
    X(timestamp) \
    X(date) \
    X(counter) \
    X(color) \
    X(bool) \
    X(timer) \
    X(guid) \
    X(file) \
    X(signature) \
    X(rating)

#define UNIMPLEMENTED_FIELDTYPES \
    X(file) \
    X(signature) \
    X(rating)

typedef enum {
#define X(name) ft_##name,
    FIELDTYPES
#undef X
    FIELD_TYPE_LENGTH,
} FieldType;

#define ASSERT_FIELD_TYPES_LENGTH(actual_length) \
    static_assert(FIELD_TYPE_LENGTH == actual_length, "Missing FieldType handler")

typedef enum {
    ts_null, ts_true, ts_false
} Tristate;

typedef struct {
    const char *label;
    bool required;
    const char *placeholder;
    size_t maxlength;
    CompiledRegex regex;
    const char *pattern;
} TextFieldMembers;

typedef struct {
    const char *label;
    bool required;
    double min;
    double max;
    double step;
} NumberFieldMembers;

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} SelectOptions;

typedef struct {
    const char *label;
    bool required;
    SelectOptions options;
} SelectFieldMembers;

typedef struct {
    const char *label;
    SelectOptions options;
    int min;
    int max;
} MultiSelectFieldMembers;

typedef struct {
    const char *label;
    bool required;
} RequiredQFieldMembers;

typedef struct {
    const char *label;
} QOnlyFieldMembers;

typedef struct {
    bool is_today;
    struct tm *dt;
} date_t;

typedef struct {
    const char *label;
    bool required;
    date_t start_date;
    date_t end_date;
} DateFieldMembers;

typedef struct {
    const char *label;
    bool required;
    const char *placeholder;
    int min;
    int max;
    size_t maxlength;
    CompiledRegex regex;
    const char *pattern;
} MultiTextFieldMembers;

typedef struct {
    const char *label;
    size_t maxsize;
    int min;
    int max;
    SelectOptions fileexts;
} FileFieldMembers;

typedef enum {
    mr_five = 5,
    mr_ten = 10,
} MaxRating;

typedef struct {
    const char *label;
    bool required;
    MaxRating maxrating;
} RatingFieldMembers;

ASSERT_FIELD_TYPES_LENGTH(15);
typedef struct {
    const char *id;
    FieldType type;
    union {
        TextFieldMembers text;
        NumberFieldMembers number;
        SelectFieldMembers select;
        MultiSelectFieldMembers multiselect;
        MultiTextFieldMembers multitext;
        DateFieldMembers date;
        QOnlyFieldMembers counter;
        QOnlyFieldMembers color;
        RequiredQFieldMembers boolean;
        QOnlyFieldMembers timer;
        FileFieldMembers file;
        RequiredQFieldMembers signature;
        RatingFieldMembers rating;
    };
} Field;

typedef struct {
    Field *items;
    size_t count;
    size_t capacity;
} Fields;

typedef struct {
    const char *id;
    const char *title;
    Fields fields;
} Form;

typedef struct {
    const char *id;
    FieldType type;
    union {
        const char *value;
        SelectOptions options;
    };
} Answer;

typedef struct {
    Answer *items;
    size_t count;
    size_t capacity;
} Answers;

typedef struct {
    const char *file_path;
    int pretty_print;
} AppConfig;

#endif
