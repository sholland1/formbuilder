#define JIM_IMPLEMENTATION
#define JIMP_IMPLEMENTATION
#define NOB_IMPLEMENTATION
#define NOB_UNSTRIP_PREFIX
#include "jim_form.h"

#include <termios.h>
#include <locale.h>
#include <pthread.h>
#include <inttypes.h>

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

#define BUFFER_LEN 1024

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CLAMP(x, lower, upper) (MAX((lower), MIN((upper), (x))))
#define BETWEEN(x, lower, upper) ((x) >= (lower) && (x) <= (upper))

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
    static char quoted_answer_buffer[BUFFER_LEN+2];
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

bool is_match(const regex_t *regex, const char *s) {
    return !regex || !regexec(regex, s, 0, NULL, 0);
}

bool is_numeric(const char *str) {
    if (str == NULL || *str == '\0') return false;

    // Allow optional leading sign
    if (*str == '+' || *str == '-') str++;

    bool has_dot = false;
    bool has_digit = false;

    while (*str) {
        if (isdigit((uint8_t)*str)) {
            has_digit = true;
        }
        else if (*str == '.') {
            if (has_dot) return false;
            has_dot = true;
        }
        else {
            return false;
        }
        str++;
    }

    return has_digit;
}

bool is_empty(const char *s) {
    return !s || !*s;
}

double to_double(const char* str) {
    return str ? strtod(str, NULL) : 0.0;
}
uint64_t to_int(const char* str) {
    return str ? strtoul(str, NULL, 10) : 0;
}

bool fails_multiselect_checks(const Field *f, uint32_t opts_count) {
    NOB_ASSERT(f->type == ft_multiselect);
    MultiSelectFieldMembers p = f->multiselect;
    return !BETWEEN(opts_count, p.min, p.max);
}

bool fails_number_checks(const Field *f, const char *answer) {
    NOB_ASSERT(f->type == ft_number);
    NumberFieldMembers p = f->number;
    bool empty = is_empty(answer);
    bool req = p.required;
    bool isnum = !empty && is_numeric(answer);
    bool between = isnum && BETWEEN(to_double(answer), p.min, p.max);
    return (between && !isnum && !req) || (isnum && !between && !req) || (!empty && !isnum && !req);
}

static struct termios original_termios;
static FILE *tty_in = NULL;
static FILE *tty_out = NULL;

void write_nl(FILE *stream) {
    putc('\r', stream);
    putc('\n', stream);
    fflush(stream);
}

void user_exit(void) {
    write_nl(tty_out);
    exit(EXIT_FAILURE);
}

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

Key read_key(FILE *stream) {
    uint8_t buffer[6];
    int count = read(fileno(stream), &buffer, 6);
    if (count == 0) return KEY_EOF;

    uint8_t c = buffer[0];
    if (count == 1) {
        if (c == 3 || c == 4) return KEY(key_exit); // Ctrl+C or Ctrl+D
        if (c == '\n' || c == '\r') return KEY(key_enter);
        if (c == '\t') return KEY(key_tab);
        if (c >= 32 && c <= 126) return (Key){.type = key_char, .ch = c}; // Printable ASCII
        if (c == 127 || c == '\b') return KEY(key_backspace);
        if (c == 23) return KEY(key_ctrl_backspace);
        if (c == '\033') return KEY(key_escape);
        return KEY(key_unknown);
    }
    if (c != '\033') return KEY(key_unknown);

    c = buffer[1];
    if (count == 2 && c == 'd') return KEY(key_ctrl_delete);
    if (c != '[') return KEY(key_unknown);

    c = buffer[2];
    if (count == 3) {
        if (c == 'A') return KEY(key_arrow_up);
        if (c == 'B') return KEY(key_arrow_down);
        if (c == 'C') return KEY(key_arrow_right);
        if (c == 'D') return KEY(key_arrow_left);
        if (c == 'F') return KEY(key_end);
        if (c == 'H') return KEY(key_home);
        if (c == 'Z') return KEY(key_shift_tab);
        return KEY(key_unknown);
    }

    if (count == 4 && c == '3' && buffer[3] == '~') return KEY(key_delete);
    else if (count == 6 && c == '1' && buffer[3] == ';' && buffer[4] == '5') {
        c = buffer[5];
        if (c == 'C') return KEY(key_ctrl_right);
        if (c == 'D') return KEY(key_ctrl_left);
    }
    return KEY(key_unknown);
}

bool is_word_boundary(char c) {
    return c == ' ' || c == '.';
}

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

void color_to_str(char* buffer, Color c) {
    sprintf(buffer, "#%.2X%.2X%.2X", c.r, c.g, c.b);
}

#define RGB_SET(rgb, component, value)  \
    ( (rgb) & ~(0xFu << (component)) ) | ( ((value) & 0xFu) << (component) )

Color read_color(const Field *f) {
    NOB_ASSERT(f->type == ft_color);

    static const uint8_t component_locations[] = {3, 4, 8, 9, 13, 14};

    fprintf(tty_out, "%s\r\n "RED"R"RESET"    "GREEN"G"RESET"    "BLUE"B"RESET"\r\n", f->color.question);

    Color c = {0};
    int pos = 1;
    while (1) {
        fprintf(tty_out, "\r"CLRDOWN"0x%.2X 0x%.2X 0x%.2X : "RGB_BG"   "RESET, c.r, c.g, c.b, c.r, c.g, c.b);
        fprintf(tty_out, "\r"RIGHT(%d), component_locations[pos]);
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter) {
            write_nl(tty_out);
            return c;
        }

        if (k.type == key_tab) {
            static const int next_pos[] = {3, 3, 5, 5, 1, 1};
            pos = next_pos[pos];
        }
        else if (k.type == key_shift_tab) {
            static const int prev_pos[] = {5, 5, 1, 1, 3, 3};
            pos = prev_pos[pos];
        }
        else if (k.type == key_arrow_left) {
            if (pos == 0) pos = 5;
            else pos--;
        }
        else if (k.type == key_arrow_right) {
            if (pos == 5) pos = 0;
            else pos++;
        }
        else if (k.type == key_arrow_up) {
            if (pos == 0) c.r+=16;
            else if (pos == 1) c.r++;
            else if (pos == 2) c.g+=16;
            else if (pos == 3) c.g++;
            else if (pos == 4) c.b+=16;
            else if (pos == 5) c.b++;
        }
        else if (k.type == key_arrow_down) {
            if (pos == 0) c.r-=16;
            else if (pos == 1) c.r--;
            else if (pos == 2) c.g-=16;
            else if (pos == 3) c.g--;
            else if (pos == 4) c.b-=16;
            else if (pos == 5) c.b--;
        }
        else if (k.type == key_char) {
            if (isxdigit(k.ch)) {
                char x = tolower(k.ch);
                x -= isdigit(x) ? '0' : ('a'-10);
                static const uint8_t pos_to_shift[] = {4, 0, 12, 8, 20, 16};
                c.rgb = RGB_SET(c.rgb, pos_to_shift[pos], x);
            }
        }
    }
}

void set_default_date(struct tm *result, const date_t *d, const struct tm* current) {
    if (d->is_today) {
        result->tm_year = current->tm_year;
        result->tm_mon = current->tm_mon;
        result->tm_mday = current->tm_mday;
    }
    else if (d->dt != NULL) {
        struct tm *x = d->dt;
        result->tm_year = x->tm_year;
        result->tm_mon = x->tm_mon;
        result->tm_mday = x->tm_mday;
    }
    else {
        result->tm_year = 0;
        result->tm_mon = 0;
        result->tm_mday = 1;
    }
    result->tm_hour = 12;
}

bool is_leap_year(int real_year) {
    if (real_year % 4 != 0) return false;
    if (real_year % 100 != 0) return true;
    return real_year % 400 == 0;
}

int compare_dates(struct tm t1, struct tm t2) {
    time_t time1 = mktime(&t1);
    time_t time2 = mktime(&t2);

    if (time1 == (time_t)-1 || time2 == (time_t)-1) {
        return 0;  // invalid date
    }

    if      (time1 <  time2) return -1;   // date1 is earlier
    else if (time1 >  time2) return  1;   // date1 is later
    else                     return  0;   // equal
}

int days_in_month(int month, int real_year) {
    static const int monthly_days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31,
    };
    if (month == 0) return 31;
    return month == 2 && is_leap_year(real_year) ? 29 : monthly_days[month-1];
}

bool is_valid_date(int real_year, int month, int day, const struct tm *start, const struct tm *end) {
    if (month > 0 && day > 0
        && BETWEEN(day, 1, days_in_month(month, real_year))
        && BETWEEN(FAKE_YEAR(real_year), start->tm_year, end->tm_year)) {
        struct tm d = {0};
        d.tm_year = FAKE_YEAR(real_year);
        d.tm_mon = month-1;
        d.tm_mday = day;
        d.tm_hour = 12;
        return compare_dates(d, *start) >= 0 && compare_dates(d, *end) <= 0;
    }
    return false;
}

bool read_date(const Field *f, struct tm *value) {
    NOB_ASSERT(f->type == ft_date);

    static const char* month_names[] = {
        "<Month>",
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December",
    };

    int pos = 0;

    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
    struct tm start_date = {0}, end_date = {0};
    set_default_date(&start_date, &f->date.start_date, current_time);
    set_default_date(&end_date, &f->date.end_date, current_time);

    char buffer[12];
    strftime(buffer, sizeof(buffer), "%m/%d/%Y", &start_date);
    fprintf(tty_out, HIDE"%s%s : [%s - ", f->date.question, f->date.required ? "*" : "", buffer);
    strftime(buffer, sizeof(buffer), "%m/%d/%Y", &end_date);
    fprintf(tty_out, "%s]\r\n", buffer);
    fflush(tty_out);

    const uint16_t default_year = CLAMP(current_time->tm_year, start_date.tm_year, end_date.tm_year);
    uint16_t year = default_year;
    uint8_t month = 0, day = 0;

    while (1) {
        bool failed_checks = f->date.required && !is_valid_date(REAL_YEAR(year), month, day, &start_date, &end_date);
        fprintf(tty_out, "\r%s ", failed_checks ? ERR_PROMPT : PROMPT);
        if (pos == 0) fprintf(tty_out, UNDERLINE);
        fprintf(tty_out, pos == 0 ? "%9s" : "%3s" , month_names[month]);
        if (pos == 0) fprintf(tty_out, RESET);
        fprintf(tty_out, " ");

        if (pos == 1) fprintf(tty_out, UNDERLINE);
        if (day == 0) fprintf(tty_out, "dd");
        else fprintf(tty_out, "%d", day);
        if (pos == 1) fprintf(tty_out, RESET);
        fprintf(tty_out, ", ");

        if (pos == 2) fprintf(tty_out, UNDERLINE);
        fprintf(tty_out, "%d", REAL_YEAR(year));
        if (pos == 2) fprintf(tty_out, RESET);
        fprintf(tty_out, CLRDOWN);
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter) {
            if (is_valid_date(REAL_YEAR(year), month, day, &start_date, &end_date)) {
                fprintf(tty_out, "\r"PROMPT" %s %d, %d"CLRDOWN SHOW"\r\n", month_names[month], day, REAL_YEAR(year));
                fflush(tty_out);

                value->tm_year = year;
                value->tm_mon = month-1;
                value->tm_mday = day;
                value->tm_hour = 12;
                return true;
            }
            else if (!f->date.required) {
                fprintf(tty_out, "\r"ERR_PROMPT" (null)"CLRDOWN SHOW"\r\n");
                fflush(tty_out);

                return false;
            }
        }

        if (k.type == key_escape) {
            pos = 0;
            month = 0;
            day = 0;
            year = default_year;
        }
        else if (k.type == key_arrow_up) {
            if (pos == 0 && month < 12) month++;
            else if (pos == 1 && day < days_in_month(month, REAL_YEAR(year))) day++;
            else if (pos == 2 && year < end_date.tm_year) year++;
        }
        else if (k.type == key_arrow_down) {
            if (pos == 0 && month > 0) month--;
            else if (pos == 1 && day > 0) day--;
            else if (pos == 2 && year > start_date.tm_year) year--;
        }
        else if (k.type == key_arrow_left) {
            if (pos > 0) pos--;
        }
        else if (k.type == key_arrow_right) {
            if (pos < 2) pos++;
        }
    }
}

bool read_bool(const Field *f) {
    NOB_ASSERT(f->type == ft_bool);

    fprintf(tty_out, HIDE"%s\r\n", f->boolean.question);

    bool choice = true;
    while (1) {
        fprintf(tty_out, "\r%s Yes\r\n%s No\r\n",
            choice ? PROMPT : " ",
            !choice ? PROMPT : " ");
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter) {
            fprintf(tty_out, SHOW);
            return choice;
        }

        if (k.type == key_arrow_up || k.type == key_arrow_down) {
            choice = !choice;
        }
        fprintf(tty_out, UP(2) CLRDOWN);
    }
}

void read_select(const Field *f, char *buffer) {
    NOB_ASSERT(f->type == ft_select);

    SelectOptions opts = f->select.options;
    fprintf(tty_out, HIDE"%s%s\r\n", f->select.question, f->select.required ? "*" : "");

    size_t pos = 0;
    while (1) {
        fprintf(tty_out, "\r%s\r\n", pos == 0
            ? f->select.required ? ERR_PROMPT : PROMPT
            : " ");
        for (size_t i = 1; i < opts.count+1; i++) {
            fprintf(tty_out, "\r%s %s\r\n", pos == i ? PROMPT : " ", opts.items[i-1]);
        }
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter) {
            if (pos == 0 && f->select.required) {
                fprintf(tty_out, UP(%zu) CLRDOWN, opts.count+1);
                continue;
            }

            if (pos == 0) {
                buffer[0] = 0;
            }
            else {
                strcpy(buffer, opts.items[pos-1]);
            }
            fprintf(tty_out, SHOW);
            return;
        }
        if (k.type == key_tab) {
            fprintf(tty_out, SHOW);
            buffer[0] = 0;
            return;
        }

        if (k.type == key_arrow_up) {
            if (pos == 0) pos = opts.count;
            else pos--;
        }
        else if (k.type == key_arrow_down) {
            if (pos == opts.count) pos = 0;
            else pos++;
        }
        fprintf(tty_out, UP(%zu) CLRDOWN, opts.count+1);
    }
}

void read_multiselect(const Field *f, SelectOptions *selected_opts) {
    NOB_ASSERT(f->type == ft_multiselect);

    MultiSelectFieldMembers p = f->multiselect;
    fprintf(tty_out, HIDE"%s ", p.question);
    if (p.max == UINT_MAX) {
        if (p.min == 0) fprintf(tty_out, "(any)\r\n");
        else fprintf(tty_out, "(at least %d)\r\n", p.min);
    }
    else fprintf(tty_out, "(%d-%d)\r\n", p.min, p.max);

    const SelectOptions opts = p.options;
    bool selected_indexes[100] = {0};
    size_t pos = 0;
    while (1) {
        int selected_opt_count = 0;
        for (size_t i = 0; i < opts.count; i++) {
            if (selected_indexes[i]) {
                selected_opt_count++;
            }
        }
        bool fail_checks = fails_multiselect_checks(f, selected_opt_count);
        for (size_t i = 0; i < opts.count; i++) {
            fprintf(tty_out, "\r%s %s %s\r\n",
                pos == i
                    ? fail_checks ? ERR_PROMPT : PROMPT
                    : " ",
                selected_indexes[i] ? CHECK : UNCHECK,
                opts.items[i]);
        }
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter || k.type == key_tab) {
            if (fail_checks) {
                fprintf(tty_out, UP(%zu) CLRDOWN, opts.count);
                continue;
            }
            fprintf(tty_out, SHOW);
            for (size_t i = 0; i < opts.count; i++) {
                if (selected_indexes[i]) {
                    nob_da_append(selected_opts, opts.items[i]);
                }
            }
            return;
        }
        if (k.type == key_char && k.ch == ' ') {
            selected_indexes[pos] = !selected_indexes[pos];
        }

        if (k.type == key_arrow_up) {
            if (pos == 0) pos = opts.count-1;
            else pos--;
        }
        else if (k.type == key_arrow_down) {
            if (pos == opts.count-1) pos = 0;
            else pos++;
        }
        fprintf(tty_out, UP(%zu) CLRDOWN, opts.count);
    }
}

uint64_t read_counter(const Field *f) {
    NOB_ASSERT(f->type == ft_counter);

    fprintf(tty_out, HIDE"%s\r\n0", f->counter.question);
    fflush(tty_out);

    uint64_t value = 0;

    while (1) {
        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter) {
            fprintf(tty_out, SHOW"\r\n");
            fflush(tty_out);
            return value;
        }

        if (k.type == key_escape) {
            value = 0;
        }
        else if (k.type == key_arrow_up || (k.type == key_char && k.ch == ' ')) {
            if (value < UINT64_MAX) value++;
        }
        else if (k.type == key_arrow_down) {
            if (value > 0) value--;
        }
        else continue;

        fprintf(tty_out, "\r%lu"CLRDOWN, value);
        fflush(tty_out);
    }
}

typedef struct {
    size_t position;
    size_t end;
    char *buffer;
} TextBuffer;

bool fails_text_checks(const Field *f, char *answer) {
    NOB_ASSERT(f->type == ft_text);
    TextFieldMembers p = f->text;
    return is_empty(answer)
        ? p.required
        : !is_match(p.regex, answer);
}

void place_char_in_text_buffer(TextBuffer *tb, char c) {
    if (tb->position < tb->end) {
        for (size_t i = tb->end+1; i >= tb->position+1; i--) {
            tb->buffer[i] = tb->buffer[i-1];
        }
    }
    tb->buffer[tb->position++] = c;
    tb->buffer[++tb->end] = '\0';
}

void reset_text_buffer(TextBuffer *tb) {
    tb->buffer[0] = '\0';
    tb->position = 0;
    tb->end = 0;
}

void handle_text_buffer(TextBuffer *tb, KeyType kt) {
    if (kt == key_escape) {
        reset_text_buffer(tb);
    }
    else if (kt == key_backspace) {
        if (tb->position > 0) {
            for (size_t i = tb->position; i < tb->end; i++) {
                tb->buffer[i-1] = tb->buffer[i];
            }
            tb->position--;
            tb->buffer[--tb->end] = '\0';
        }
    }
    else if (kt == key_ctrl_backspace) {
        int orig_pos = tb->position;
        while (tb->position > 0 && is_word_boundary(tb->buffer[tb->position-1])) tb->position--;
        while (tb->position > 0 && !is_word_boundary(tb->buffer[tb->position-1])) tb->position--;
        int del_len = orig_pos-tb->position;
        for (size_t i = orig_pos; i < tb->end; i++) {
            tb->buffer[i-del_len] = tb->buffer[i];
        }
        tb->end -= del_len;
        tb->buffer[tb->end] = '\0';
    }
    else if (kt == key_delete) {
        if (tb->position < tb->end) {
            for (size_t i = tb->position + 1; i < tb->end; i++) {
                tb->buffer[i-1] = tb->buffer[i];
            }
            tb->buffer[--tb->end] = '\0';
        }
    }
    else if (kt == key_ctrl_delete) {
        int orig_pos = tb->position;
        while (tb->position < tb->end && !is_word_boundary(tb->buffer[tb->position])) tb->position++;
        while (tb->position < tb->end && is_word_boundary(tb->buffer[tb->position])) tb->position++;
        int del_len = tb->position-orig_pos;
        for (size_t i = tb->position; i < tb->end; i++) {
            tb->buffer[i-del_len] = tb->buffer[i];
        }
        tb->end -= del_len;
        tb->position = orig_pos;
        tb->buffer[tb->end] = '\0';
    }
    else if (kt == key_home) {
        tb->position = 0;
    }
    else if (kt == key_end) {
        tb->position = tb->end;
    }
    else if (kt == key_arrow_right) {
        if (tb->position < tb->end) tb->position++;
    }
    else if (kt == key_arrow_left) {
        if (tb->position > 0) tb->position--;
    }
    else if (kt == key_ctrl_right) {
        while (tb->position < tb->end && !is_word_boundary(tb->buffer[tb->position])) tb->position++;
        while (tb->position < tb->end && is_word_boundary(tb->buffer[tb->position])) tb->position++;
    }
    else if (kt == key_ctrl_left) {
        while (tb->position > 0 && is_word_boundary(tb->buffer[tb->position-1])) tb->position--;
        while (tb->position > 0 && !is_word_boundary(tb->buffer[tb->position-1])) tb->position--;
    }
}

void read_text(const Field *f, char *buffer) {
    NOB_ASSERT(f->type == ft_text);

    TextBuffer tb = {
        .position = 0,
        .end = 0,
        .buffer = buffer,
    };

    fprintf(tty_out, "%s%s\r\n", f->text.question, f->text.required ? "*" : "");

    const char *ph = f->text.placeholder;
    while (1) {
        bool failed_checks = fails_text_checks(f, buffer);
        fprintf(tty_out, "\r%s ", failed_checks ? ERR_PROMPT : PROMPT);
        if (tb.end == 0 && ph)
            fprintf(tty_out, FAINT"%s"RESET, ph);
        else
            fprintf(tty_out, "%s", buffer);
        fprintf(tty_out, CLRDOWN"\r"RIGHT(%zu), tb.position+3);
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter) {
            if (failed_checks) {
                reset_text_buffer(&tb);
                continue;
            }
            tb.buffer[tb.end] = '\0';
            write_nl(tty_out);
            return;
        }

        if (k.type == key_char && tb.position < f->text.maxlength - 1) {
            place_char_in_text_buffer(&tb, k.ch);
            continue;
        }

        handle_text_buffer(&tb, k.type);
    }
}

void read_number(const Field *f, char *buffer) {
    NOB_ASSERT(f->type == ft_number);

    TextBuffer tb = {
        .position = 0,
        .end = 0,
        .buffer = buffer,
    };

    NumberFieldMembers p = f->number;
    fprintf(tty_out, "%s%s\r\n", p.question, p.required ? "*" : "");

    while (1) {
        bool failed_checks = fails_number_checks(f, buffer);
        fprintf(tty_out, "\r%s %s" CLRDOWN"\r"RIGHT(%zu), failed_checks ? ERR_PROMPT : PROMPT, buffer, tb.position+3);
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter) {
            if (failed_checks) {
                reset_text_buffer(&tb);
                continue;
            }
            tb.buffer[tb.end] = '\0';
            write_nl(tty_out);
            return;
        }

        if (k.type == key_char && (k.ch == '-' || k.ch == '.' || isdigit(k.ch))) {
            // if (pos < BUFFER_LEN - 1) {
            place_char_in_text_buffer(&tb, k.ch);
            continue;
        }

        double signed_step;
        if (k.type == key_arrow_up) {
            signed_step = p.step;
        }
        else if (k.type == key_arrow_down) {
            signed_step = -p.step;
        }
        else {
            handle_text_buffer(&tb, k.type);
            continue;
        }

        double current = round(to_double(buffer) / p.step) * p.step;
        snprintf(buffer, BUFFER_LEN, "%g", CLAMP(current + signed_step, p.min, p.max));
        tb.position = strlen(buffer);
        tb.end = tb.position;
    }
}

void terminal_deinit(void) {
    if (tty_in) {
        tcsetattr(fileno(tty_in), TCSAFLUSH, &original_termios);
        fclose(tty_in);
        tty_in = NULL;
    }
    if (tty_out) {
        fprintf(tty_out, RESET SHOW);
        fflush(tty_out);
        fclose(tty_out);
        tty_out = NULL;
    }
}

void terminal_init(void) {
    // Open controlling terminal
    tty_in = fopen("/dev/tty", "r");
    tty_out = fopen("/dev/tty", "w");
    if (!tty_in || !tty_out) {
        perror("Failed to open /dev/tty");
        exit(EXIT_FAILURE);
    }

    // Save original settings
    if (tcgetattr(fileno(tty_in), &original_termios) == -1) {
        perror("tcgetattr");
        terminal_deinit();           // best effort cleanup
        exit(EXIT_FAILURE);
    }

    struct termios raw = original_termios;

    // Disable echo, canonical mode, signals (^C generates SIGINT)
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);

    // Input flags
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    // Output flags — very important for many curses-like programs
    raw.c_oflag &= ~(OPOST);

    // Character size
    raw.c_cflag |= CS8;

    // Timing — read returns immediately after 1 byte
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(fileno(tty_in), TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        terminal_deinit();
        exit(EXIT_FAILURE);
    }

    // Register normal exit cleanup
    if (atexit(terminal_deinit) != 0) {
        fprintf(stderr, "atexit failed\n");
        terminal_deinit();
        exit(EXIT_FAILURE);
    }
}

volatile bool running = false;
volatile uint64_t nanoseconds_total = 0;

#define NANOSEC_PER_SEC 1000000000ULL
#define NANOSEC_PER_MIN  (60 * NANOSEC_PER_SEC)
#define NANOSEC_PER_HOUR (60 * NANOSEC_PER_MIN)
#define NANOSEC_PER_CENTISEC 10000000ULL
#define TIMER_SLEEP_TIME 10000
void fprint_timer(FILE *stream, uint64_t total) {
    uint64_t hours   = total / NANOSEC_PER_HOUR;
    uint64_t minutes = (total % NANOSEC_PER_HOUR) / NANOSEC_PER_MIN;
    uint64_t seconds = (total % NANOSEC_PER_MIN) / NANOSEC_PER_SEC;
    uint64_t centi   = (total % NANOSEC_PER_SEC) / NANOSEC_PER_CENTISEC;

    fprintf(stream, "\r%02lu:%02lu:%02lu:%02lu"CLRDOWN, hours, minutes, seconds, centi);
    fflush(stream);
}

void* timer_thread(void* arg) {
    NOB_UNUSED(arg);

    running = true;
    struct timespec last, now;
    clock_gettime(CLOCK_MONOTONIC, &last);
    while (running) {
        clock_gettime(CLOCK_MONOTONIC, &now);

        // Calculate elapsed time since last update (in nanoseconds)
        uint64_t elapsed_ns = (now.tv_sec - last.tv_sec) * NANOSEC_PER_SEC +
                                (now.tv_nsec - last.tv_nsec);

        if (elapsed_ns > 0) {
            nanoseconds_total += elapsed_ns;
            last = now;
        }
        fprint_timer(tty_out, nanoseconds_total);
        usleep(TIMER_SLEEP_TIME);
    }
    return NULL;
}

uint64_t read_timer(const Field *f) {
    NOB_ASSERT(f->type == ft_timer);

    pthread_t timer_tid;
    fprintf(tty_out, HIDE"%s\r\n", f->timer.question);
    fprintf(tty_out, "Press [space] to start, [esc] to reset, or [enter] to submit.\r\n");
    fprint_timer(tty_out, nanoseconds_total);

    while (1) {
        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();

        if (running) {
            if (k.type == key_char && k.ch == ' ') {
                // Signal timer to stop
                running = false;

                fprintf(tty_out, UP(1)"\rTimer paused. Press [space] to start, [esc] to reset, or [enter] to submit."CLRDOWN"\r\n");
                fprint_timer(tty_out, nanoseconds_total);

                // Wait for timer thread to finish
                pthread_join(timer_tid, NULL);
            }
            continue;
        }

        if (k.type == key_enter) {
            fprintf(tty_out, "\r\n"SHOW);
            fflush(tty_out);
            return nanoseconds_total;
        }

        if (k.type == key_escape) {
            nanoseconds_total = 0;
            fprintf(tty_out, UP(1)"\rTimer paused. Press [space] to start, [esc] to reset, or [enter] to submit."CLRDOWN"\r\n");
            fprint_timer(tty_out, nanoseconds_total);
            continue;
        }

        if (k.type == key_char && k.ch == ' ') {
            fprintf(tty_out, UP(1)"\rTimer running. Press [space] to stop."CLRDOWN"\r\n");
            fprint_timer(tty_out, nanoseconds_total);
            // Start timer thread
            if (pthread_create(&timer_tid, NULL, timer_thread, NULL) != 0) {
                perror("Failed to create timer thread");
                return -1;
            }
        }
    }
}


int ns_to_iso8601_duration(uint64_t total_ns, char *buf, size_t bufsize) {
    NOB_ASSERT(bufsize > 32);
    uint64_t hours   = total_ns / NANOSEC_PER_HOUR;
    uint64_t minutes = (total_ns % NANOSEC_PER_HOUR) / NANOSEC_PER_MIN;
    uint64_t seconds = (total_ns % NANOSEC_PER_MIN) / NANOSEC_PER_SEC;
    uint64_t centi   = (total_ns % NANOSEC_PER_SEC) / NANOSEC_PER_CENTISEC;

    char *p = buf;
    char *end = buf + bufsize;

    *p++ = 'P';
    *p++ = 'T';   // we never need days or higher here

    bool wrote_something = false;

    if (hours > 0) {
        int n = snprintf(p, end - p, "%" PRIu64 "H", hours);
        if (n <= 0) return -1;
        p += n;
        wrote_something = true;
    }

    if (minutes > 0) {
        int n = snprintf(p, end - p, "%" PRIu64 "M", minutes);
        if (n <= 0) return -1;
        p += n;
        wrote_something = true;
    }

    // Always write seconds part if > 0 or if we have fraction
    if (seconds > 0 || centi > 0 || !wrote_something) {
        if (centi > 0) {
            int n = snprintf(p, end - p, "%" PRIu64 ".%02" PRIu64 "S",
                             seconds, centi);
            if (n <= 0) return -1;
            p += n;
        } else {
            int n = snprintf(p, end - p, "%" PRIu64 "S", seconds);
            if (n <= 0) return -1;
            p += n;
        }
        wrote_something = true;
    }

    // Edge case: exactly zero duration
    if (!wrote_something) {
        snprintf(buf, bufsize, "PT0S");
        return 4;
    }

    *p = '\0';
    return (int)(p - buf);
}

void display_form(const Form *form, Answers *answers) {
    fprintf(tty_out, CLR HOME BOLD"%s"RESET"\r\n", form->title);

    static char answer_buffer[BUFFER_LEN];
    const char *timestamp_field_id = NULL;
    nob_da_foreach(Field, f, &form->fields) {
        answer_buffer[0] = '\0';

        switch (f->type) {
        case ft_text: {
            read_text(f, answer_buffer);
            if (is_empty(answer_buffer)) {
                append_null_answer(answers, f->id);
            }
            else {
                append_quoted_answer(answers, f->id, answer_buffer);
            }
        } break;

        case ft_number: {
            read_number(f, answer_buffer);
            if (is_empty(answer_buffer)) {
                append_null_answer(answers, f->id);
            }
            else {
                append_raw_answer(answers, f->id, answer_buffer);
            }
        } break;

        case ft_select: {
            read_select(f, answer_buffer);
            if (is_empty(answer_buffer)) {
                append_null_answer(answers, f->id);
            }
            else {
                append_quoted_answer(answers, f->id, answer_buffer);
            }
        } break;

        case ft_multiselect: {
            SelectOptions opts = {0};
            read_multiselect(f, &opts);
            append_multiselect_answer(answers, f->id, &opts);
        } break;

        case ft_date: {
            struct tm d;
            if (read_date(f, &d)) {
                strftime(answer_buffer, sizeof(answer_buffer), "%Y-%m-%d", &d);
                append_quoted_answer(answers, f->id, answer_buffer);
            }
            else {
                append_null_answer(answers, f->id);
            }
        } break;

        case ft_counter: {
            sprintf(answer_buffer, "%lu", read_counter(f));
            append_raw_answer(answers, f->id, answer_buffer);
        } break;

        case ft_color: {
            color_to_str(answer_buffer, read_color(f));
            append_quoted_answer(answers, f->id, answer_buffer);
        } break;

        case ft_bool: {
            append_static_answer(answers, f->id, read_bool(f) ? "true" : "false");
        } break;

        case ft_timer: {
            uint64_t duration_in_nanoseconds = read_timer(f);
            ns_to_iso8601_duration(duration_in_nanoseconds, answer_buffer, BUFFER_LEN);
            append_quoted_answer(answers, f->id, answer_buffer);
        } break;

        case ft_timestamp:
            timestamp_field_id = f->id;
            break;

        default: NOB_UNREACHABLE("Unidentified type!");
        }
    }

    if (timestamp_field_id) {
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        strftime(answer_buffer, sizeof(answer_buffer), "%Y-%m-%d %H:%M:%S", t);
        append_quoted_answer(answers, timestamp_field_id, answer_buffer);
    }
}

int main(void) {
    const char *file_path = "test.json";
    Option option = pretty;
    Nob_String_Builder sb = {0};
    if (!nob_read_entire_file(file_path, &sb)) return 1;

    Jimp jimp = {0};
    jimp_begin(&jimp, file_path, sb.items, sb.count);

    Form form = {0};
    if (!jimp_form(&jimp, &form)) {
        fprintf(stderr, "Failed to parse form in %s", file_path);
        return 1;
    }

    terminal_init();

    Answers answers = {0};
    nob_da_reserve(&answers, form.fields.count);

    display_form(&form, &answers);

    terminal_deinit();

    // This necessary to output json double correctly
    setlocale(LC_NUMERIC, "C");

    Jim jim = {.pp = option == pretty ? 4 : 0};
    jim_answers(&jim, &answers);
    fwrite(jim.sink, jim.sink_count, 1, stdout);

    return 0;
}
