#ifndef FORM_CLI_H
#define FORM_CLI_H

#ifndef NOB_H_
#include "nob.h"
#endif

#ifndef TYPES_H
#include "types.h"
#endif

#ifndef REGEX_H
#include "regex.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <termios.h>
#include <time.h>

#ifndef REAL_YEAR
#define REAL_YEAR(year) (NOB_ASSERT((year) < 1900), (year)+1900)
#endif

#ifndef FAKE_YEAR
#define FAKE_YEAR(year) (NOB_ASSERT((year) >= 1900), (year)-1900)
#endif

#define CLR       "\033[2J"
#define HOME      "\033[H"
#define RESET     "\033[0m"
#define BOLD      "\033[1m"
#define FAINT     "\033[2m"
#define UNDERLINE "\033[4m"
#define RED       "\033[31m"
#define GREEN     "\033[32m"
#define BLUE      "\033[34m"
#define CLRDOWN   "\033[K"
#define HIDE      "\033[?25l"
#define SHOW      "\033[?25h"
#define UP(n)     "\033["#n"A"
#define RIGHT(n)  "\033["#n"G"

#define PROMPT     BOLD"→"RESET
#define ERR_PROMPT RED PROMPT

#define CHECK   "[✘]"
#define UNCHECK "[ ]"

#define RGB_FG "\033[38;2;%d;%d;%dm"
#define RGB_BG "\033[48;2;%d;%d;%dm"

#define ANSWER_BUFFER_LEN 1024

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CLAMP(x, lower, upper) (MAX((lower), MIN((upper), (x))))
#define BETWEEN(x, lower, upper) ((x) >= (lower) && (x) <= (upper))

typedef enum {
    key_eof,
    key_char,
    key_delete,
    key_ctrl_delete,
    key_backspace,
    key_ctrl_backspace,
    key_escape,
    key_enter,
    key_tab,
    key_shift_tab,
    key_arrow_up,
    key_arrow_down,
    key_arrow_right,
    key_arrow_left,
    key_shift_up,
    key_shift_down,
    key_shift_right,
    key_shift_left,
    key_home,
    key_end,
    key_ctrl_right,
    key_ctrl_left,
    key_exit,
    key_unknown,
} KeyType;

typedef struct {
    KeyType type;
    uint8_t ch;
} Key;

#define KEY(x) (Key){.type = (x)}
#define KEY_EOF KEY(key_eof)

typedef struct {
    union {
        struct {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };
        uint32_t rgb;
    };
} Color;

typedef struct {
    size_t position;
    size_t end;
    char *buffer;
} TextBuffer;

extern struct termios original_termios;
extern FILE *tty_in;
extern FILE *tty_out;

void append_raw_answer(Answers *answers, const char *id, const char *value);
void append_static_answer(Answers *answers, const char *id, const char *value);
void append_null_answer(Answers *answers, const char *id);
void append_quoted_answer(Answers *answers, const char *id, const char *value);
void append_multiselect_answer(Answers *answers, const char *id, SelectOptions *opts);
void append_multitext_answer(Answers *answers, const char *id, const char *values);

bool is_empty(const char *s);

void write_nl(FILE *stream);
void user_exit(void);
Key read_key(FILE *stream);
void terminal_init(void);
void terminal_deinit(void);

void color_to_str(char *buffer, Color c);
Color read_color(const Field *f);
bool read_date(const Field *f, struct tm *value);
Tristate read_bool(const Field *f);
void read_select(const Field *f, char *buffer);
void read_multiselect(const Field *f, SelectOptions *selected_opts);
int64_t read_counter(const Field *f);
void read_text(const Field *f, char *buffer);
void read_number(const Field *f, char *buffer);
void read_multitext(const Field *f, char *buffer);
void sprint_score(char *buffer, Rating r);
Rating read_rating(const Field *f);

uint64_t read_timer(const Field *f);
int ns_to_iso8601_duration(uint64_t total_ns, char *buf, size_t bufsize);

#endif
