#include <regex.h>
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
    X(bool)

typedef enum {
#define X(name) ft_##name,
    FIELDTYPES
#undef X
    FIELD_TYPE_LENGTH,
} FieldType;

typedef struct {
    const char *question;
    const char *placeholder;
    size_t maxlength;
    bool required;
    regex_t *regex;
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
    uint32_t min;
    uint32_t max;
} MultiSelectFieldMembers;

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
    const char *id;
    FieldType type;
    union {
        TextFieldMembers text;
        NumberFieldMembers number;
        SelectFieldMembers select;
        MultiSelectFieldMembers multiselect;
        DateFieldMembers date;
        QOnlyFieldMembers counter;
        QOnlyFieldMembers color;
        QOnlyFieldMembers boolean;
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

typedef enum {
    none, one, pretty
} Option;
