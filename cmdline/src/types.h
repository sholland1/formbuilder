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
    X(guid)

typedef enum {
#define X(name) ft_##name,
    FIELDTYPES
#undef X
    FIELD_TYPE_LENGTH,
} FieldType;

typedef enum {
    ts_null, ts_true, ts_false
} Tristate;

typedef struct {
    const char *question;
    bool required;
    const char *placeholder;
    size_t maxlength;
    CompiledRegex regex;
    const char *pattern;
} TextFieldMembers;

typedef struct {
    const char *question;
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
    const char *question;
    bool required;
    SelectOptions options;
} SelectFieldMembers;

typedef struct {
    const char *question;
    SelectOptions options;
    int min;
    int max;
} MultiSelectFieldMembers;

typedef struct {
    const char *question;
    bool required;
} BoolFieldMembers;

typedef struct {
    const char *question;
} QOnlyFieldMembers;

typedef struct {
    bool is_today;
    struct tm *dt;
} date_t;

typedef struct {
    const char *question;
    bool required;
    date_t start_date;
    date_t end_date;
} DateFieldMembers;

typedef struct {
    const char *question;
    bool required;
    const char *placeholder;
    int min;
    int max;
    size_t maxlength;
    CompiledRegex regex;
    const char *pattern;
} MultiTextFieldMembers;

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
        BoolFieldMembers boolean;
        QOnlyFieldMembers timer;
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
