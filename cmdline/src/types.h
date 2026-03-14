#include <regex.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ft_text,
    ft_number,
    ft_select,
    ft_multiselect,
    ft_multitext,
    ft_timestamp,
    ft_date,
    ft_counter,
    ft_color,
    ft_bool,
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
    const char *question;
} BoolFieldMembers;

typedef struct {
    const char *id;
    FieldType type;
    union {
        TextFieldMembers text;
        NumberFieldMembers number;
        BoolFieldMembers boolean;
    };
} Field;

typedef struct {
    Field *items;
    size_t capacity;
    size_t count;
} Fields;

typedef struct {
    const char *id;
    const char *title;
    Fields fields;
} Form;

typedef struct {
    const char *id;
    const char *value;
} Answer;

typedef struct {
    Answer *items;
    size_t capacity;
    size_t count;
} Answers;

typedef enum {
    none, one, pretty
} Option;
