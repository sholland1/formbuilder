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
    X(rating) \
    X(group)

#define UNIMPLEMENTED_FIELDTYPES \
    X(file) \
    X(signature)

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
    float score;
    MaxRating max_score;
} Rating;

#define MAKE_RATING(s, ms) (Rating){ (s), (ms) }

typedef struct {
    const char *label;
    bool required;
    MaxRating maxrating;
    double step;
} RatingFieldMembers;

typedef struct _Fields Fields;

typedef struct {
    const char *label;
    Fields *fields;
} GroupFieldMembers;

ASSERT_FIELD_TYPES_LENGTH(16);
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
        GroupFieldMembers group;
    };
} Field;

struct _Fields {
    Field *items;
    size_t count;
    size_t capacity;
};

typedef enum {
    ast_nested,
    ast_flat,
    ast_basic,
} AnswerStructureType;

extern AnswerStructureType answer_structure_nested_value;
extern AnswerStructureType answer_structure_flat_value;
extern AnswerStructureType answer_structure_basic_value;

#define ANSWER_STRUCTURE_NESTED (&answer_structure_nested_value)
#define ANSWER_STRUCTURE_FLAT   (&answer_structure_flat_value)
#define ANSWER_STRUCTURE_BASIC  (&answer_structure_basic_value)

typedef struct {
    const char *id;
    const char *title;
    AnswerStructureType *answer_structure;
    Fields fields;
} Form;

typedef struct _Answers Answers;

typedef enum {
    at_string,
    at_list,
    at_nested,
} AnswerType;

typedef struct {
    const char *id;
    AnswerType type;
    union {
        const char *value;
        SelectOptions options;
        Answers *answers;
    };
} Answer;

struct _Answers {
    Answer *items;
    size_t count;
    size_t capacity;
};

#endif
