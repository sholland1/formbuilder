#define JIM_IMPLEMENTATION
#define JIMP_IMPLEMENTATION
#define NOB_IMPLEMENTATION
#define NOB_UNSTRIP_PREFIX
#include "jim_form.h"

#include <termios.h>
#include <locale.h>

#define CLR      "\e[2J"
#define HOME     "\e[H"
#define RESET    "\e[0m"
#define BOLD     "\e[1m"
#define FAINT    "\e[2m"
#define RED      "\e[31m"
#define GREEN    "\e[32m"
#define BLUE     "\e[34m"
#define CLRDOWN  "\e[K"
#define HIDE     "\e[?25l"
#define SHOW     "\e[?25h"
#define UP(n)    "\e["#n"A"
#define RIGHT(n) "\e["#n"G"

#define ERR_PROMPT RED BOLD"→"RESET
#define PROMPT BOLD"→"RESET

#define CHECK "[✘]"
#define UNCHECK "[ ]"

#define RGB_FG "\e[38;2;%d;%d;%dm"
#define RGB_BG "\e[48;2;%d;%d;%dm"

#define BUFFER_LEN 1024

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CLAMP(x, lower, upper) (MAX((lower), MIN((upper), (x))))
#define BETWEEN(x, lower, upper) ((x) >= (lower) && (x) <= (upper))

void append_answer(Answers *answers, const char *id, const char *value) {
    Answer a = {
        .id = strdup(id),
        .type = ft_text,
        .value = strdup(value),
    };
    nob_da_append(answers, a);
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
    MultiSelectFieldMembers p = f->multiselect;
    return !BETWEEN(opts_count, p.min, p.max);
}

bool fails_checks(const Field *f, const char *answer) {
    bool empty = is_empty(answer);
    switch (f->type) {
        case ft_text: {
            TextFieldMembers p = f->text;
            return empty
                ? p.required
                : !is_match(p.regex, answer);
        } break;

        case ft_number: {
            NumberFieldMembers p = f->number;
            bool req = p.required;
            bool isnum = !empty && is_numeric(answer);
            bool between = isnum && BETWEEN(to_double(answer), p.min, p.max);
            return (between && !isnum && !req) || (isnum && !between && !req) || (!empty && !isnum && !req);
        }

        case ft_select:
            return empty && f->select.required;

        case ft_counter:
        case ft_color:
        case ft_bool:
            return false;

        default: break;
    }
    NOB_UNREACHABLE("Unidentified type!");
}

static struct termios original_termios;
static FILE *tty_in = NULL;
static FILE *tty_out = NULL;

void make_prompt_red(FILE *stream, size_t pos, bool b) {
    // move to beginning of line
    putc('\r', stream);
    // put red or normal prompt
    fprintf(stream, b ? ERR_PROMPT" ": PROMPT" ");
    // move back to original position
    fprintf(stream, RIGHT(%zu), pos+3);
    fflush(stream);
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
        if (c >= 32 && c <= 126) return (Key){.type = key_char, .ch = (c)}; // Printable ASCII
        if (c == 127 || c == '\b') return KEY(key_backspace);
        if (c == 23) return KEY(key_ctrl_backspace);
        if (c == '\e') return KEY(key_escape);
        return KEY(key_unknown);
    }
    if (c != '\e') return KEY(key_unknown);

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

void write_prompt_with_buffer(FILE *stream, const char *buffer) {
    fprintf(stream, "\r"PROMPT" %s" CLRDOWN, buffer);
    fflush(stream);
}

void write_placeholder(FILE *stream, const char *ph) {
    fprintf(stream, "\r"PROMPT" "FAINT"%s"RESET"\r"RIGHT(3), ph);
    fflush(stream);
}

#define write_question(stream, field) \
    do { \
        fprintf(stream, "%s%s\r\n", (field).question, (field).required ? "*" : ""); \
        fflush(stream); \
    } while (0)

void write_nl(FILE *stream) {
    putc('\r', stream);
    putc('\n', stream);
    fflush(stream);
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

Color read_color(void) {
    static const uint8_t component_locations[] = {3, 4, 8, 9, 13, 14};

    Color c = {0};
    int pos = 1;
    fprintf(tty_out, " "RED"R"RESET"    "GREEN"G"RESET"    "BLUE"B"RESET"\r\n");
    while (1) {
        fprintf(tty_out, "\r"CLRDOWN"0x%.2X 0x%.2X 0x%.2X : "RGB_BG"   "RESET, c.r, c.g, c.b, c.r, c.g, c.b);
        fprintf(tty_out, "\r"RIGHT(%d), component_locations[pos]);
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_enter) {
            fprintf(tty_out, SHOW);
            return c;
        }
        if (k.type == key_exit) {
            fprintf(tty_out, SHOW);
            exit(EXIT_FAILURE);
        }

        if (k.type == key_tab) {
            if (pos == 0 || pos == 1) {
                pos = 3;
            }
            else if (pos == 2 || pos == 3) {
                pos = 5;
            }
            else if (pos == 4 || pos == 5) {
                pos = 1;
            }
        }
        else if (k.type == key_shift_tab) {
            if (pos == 0 || pos == 1) {
                pos = 5;
            }
            else if (pos == 2 || pos == 3) {
                pos = 1;
            }
            else if (pos == 4 || pos == 5) {
                pos = 3;
            }
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
    return c;
}

bool read_bool(void) {
    fprintf(tty_out, HIDE);

    bool choice = true;
    while (1) {
        fprintf(tty_out, "\r%s Yes\r\n%s No\r\n",
            choice ? PROMPT : " ",
            !choice ? PROMPT : " ");
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_enter) {
            fprintf(tty_out, SHOW);
            return choice;
        }
        if (k.type == key_exit) {
            fprintf(tty_out, SHOW);
            exit(EXIT_FAILURE);
        }

        if (k.type == key_arrow_up || k.type == key_arrow_down) {
            choice = !choice;
        }
        fprintf(tty_out, UP(2) CLRDOWN);
    }
}

void read_select(char *buffer, const Field *field, bool first_time) {
    fprintf(tty_out, HIDE);

    SelectOptions opts = field->select.options;
    if (!first_time)
        fprintf(tty_out, UP(%zu) CLRDOWN, opts.count+1);

    size_t pos = 0;
    while (1) {
        fprintf(tty_out, "\r%s\r\n", pos == 0
            ? field->select.required ? ERR_PROMPT : PROMPT
            : " ");
        for (size_t i = 1; i < opts.count+1; i++) {
            fprintf(tty_out, "\r%s %s\r\n", pos == i ? PROMPT : " ", opts.items[i-1]);
        }
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_enter) {
            fprintf(tty_out, SHOW);
            if (pos == 0) {
                buffer[0] = 0;
            }
            else {
                strcpy(buffer, opts.items[pos-1]);
            }
            return;
        }
        if (k.type == key_tab) {
            fprintf(tty_out, SHOW);
            buffer[0] = 0;
            return;
        }
        if (k.type == key_exit) {
            fprintf(tty_out, SHOW);
            exit(EXIT_FAILURE);
        }

        if (k.type == key_arrow_up) {
            if (pos == 0) {
                pos = opts.count;
            }
            else {
                pos--;
            }
        }
        else if (k.type == key_arrow_down) {
            if (pos == opts.count) {
                pos = 0;
            }
            else {
                pos++;
            }
        }
        fprintf(tty_out, UP(%zu) CLRDOWN, opts.count+1);
    }
}

void read_multiselect(bool *selected_indexes, SelectOptions *selected_opts, const Field *field, bool first_time) {
    fprintf(tty_out, HIDE);

    SelectOptions opts = field->multiselect.options;
    if (!first_time)
        fprintf(tty_out, UP(%zu) CLRDOWN, opts.count);

    size_t pos = 0;
    while (1) {
        int selected_opt_count = 0;
        for (size_t i = 0; i < opts.count; i++) {
            if (selected_indexes[i]) {
                selected_opt_count++;
            }
        }
        bool fail_checks = fails_multiselect_checks(field, selected_opt_count);
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
        if (k.type == key_enter || k.type == key_tab) {
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
        if (k.type == key_exit) {
            fprintf(tty_out, SHOW);
            exit(EXIT_FAILURE);
        }

        if (k.type == key_arrow_up) {
            if (pos == 0) {
                pos = opts.count-1;
            }
            else {
                pos--;
            }
        }
        else if (k.type == key_arrow_down) {
            if (pos == opts.count-1) {
                pos = 0;
            }
            else {
                pos++;
            }
        }
        fprintf(tty_out, UP(%zu) CLRDOWN, opts.count);
    }
}

uint64_t read_counter() {
    fprintf(tty_out, "0"HIDE);
    fflush(tty_out);

    uint64_t value = 0;

    while (1) {
        Key k = read_key(tty_in);
        if (k.type == key_exit) {
            exit(EXIT_FAILURE);
        }
        if (k.type == key_enter) {
            fprintf(tty_out, SHOW);
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

void read_input(char *buffer, const Field *field) {
    size_t pos = 0;
    size_t end = 0;
    buffer[0] = '\0';

    while (1) {
        if (field->type == ft_text) {
            const char *ph = field->text.placeholder;
            if (end == 0 && ph) {
                write_placeholder(tty_out, ph);
            }
            else {
                write_prompt_with_buffer(tty_out, buffer);
            }
        }
        else if (field->type == ft_number) {
            write_prompt_with_buffer(tty_out, buffer);
        }
        make_prompt_red(tty_out, pos, fails_checks(field, buffer));

        Key k = read_key(tty_in);
        if (k.type == key_exit) {
            exit(EXIT_FAILURE);
        }
        if (k.type == key_enter) {
            buffer[end] = '\0';
            return;
        }

        if (k.type == key_backspace) {
            if (pos > 0) {
                for (size_t i = pos; i < end; i++) {
                    buffer[i-1] = buffer[i];
                }
                pos--;
                buffer[--end] = '\0';
                write_prompt_with_buffer(tty_out, buffer);
            }
        }
        else if (k.type == key_ctrl_backspace) {
            int orig_pos = pos;
            while (pos > 0 && is_word_boundary(buffer[pos-1])) pos--;
            while (pos > 0 && !is_word_boundary(buffer[pos-1])) pos--;
            int del_len = orig_pos-pos;
            for (size_t i = orig_pos; i < end; i++) {
                buffer[i-del_len] = buffer[i];
            }
            end -= del_len;
            buffer[end] = '\0';
            write_prompt_with_buffer(tty_out, buffer);
        }
        else if (k.type == key_delete) {
            if (pos < end) {
                for (size_t i = pos + 1; i < end; i++) {
                    buffer[i-1] = buffer[i];
                }
                buffer[--end] = '\0';
                write_prompt_with_buffer(tty_out, buffer);
            }
        }
        else if (k.type == key_ctrl_delete) {
            int orig_pos = pos;
            while (pos < end && !is_word_boundary(buffer[pos])) pos++;
            while (pos < end && is_word_boundary(buffer[pos])) pos++;
            int del_len = pos-orig_pos;
            for (size_t i = pos; i < end; i++) {
                buffer[i-del_len] = buffer[i];
            }
            end -= del_len;
            pos = orig_pos;
            buffer[end] = '\0';
            write_prompt_with_buffer(tty_out, buffer);
        }
        else if (k.type == key_home) {
            pos = 0;
        }
        else if (k.type == key_end) {
            pos = end;
        }
        else if (k.type == key_arrow_right) {
            if (pos < end) pos++;
        }
        else if (k.type == key_arrow_left) {
            if (pos > 0) pos--;
        }
        else if (k.type == key_ctrl_right) {
            while (pos < end && !is_word_boundary(buffer[pos])) pos++;
            while (pos < end && is_word_boundary(buffer[pos])) pos++;
        }
        else if (k.type == key_ctrl_left) {
            while (pos > 0 && is_word_boundary(buffer[pos-1])) pos--;
            while (pos > 0 && !is_word_boundary(buffer[pos-1])) pos--;
        }
        else if (field->type == ft_text) {
            if (k.type == key_char && pos < field->text.maxlength - 1) {
                if (pos < end) {
                    for (size_t i = end+1; i >= pos+1; i--) {
                        buffer[i] = buffer[i-1];
                    }
                    buffer[pos++] = k.ch;
                    buffer[++end] = '\0';
                    write_prompt_with_buffer(tty_out, buffer);
                }
                else {
                    buffer[pos++] = k.ch;
                    buffer[++end] = '\0';
                    putc(k.ch, tty_out);
                    fflush(tty_out);
                }
            }
        }
        else if (field->type == ft_number) {
            NumberFieldMembers p = field->number;
            if (k.type == key_char) {
                if (k.ch == '-' || k.ch == '.' || isdigit(k.ch)) {
                    // if (pos < BUFFER_LEN - 1) {
                    if (pos < end) {
                        for (size_t i = end+1; i >= pos+1; i--) {
                            buffer[i] = buffer[i-1];
                        }
                        buffer[pos++] = k.ch;
                        buffer[++end] = '\0';
                        write_prompt_with_buffer(tty_out, buffer);
                    }
                    else {
                        buffer[pos++] = k.ch;
                        buffer[++end] = '\0';
                        putc(k.ch, tty_out);
                        fflush(tty_out);
                    }
                }
            }
            else {
                double signed_step;
                if (k.type == key_arrow_up) {
                    signed_step = p.step;
                }
                else if (k.type == key_arrow_down) {
                    signed_step = -p.step;
                }
                else break;
                double current = round(to_double(buffer) / p.step) * p.step;
                snprintf(buffer, BUFFER_LEN, "%g", CLAMP(current + signed_step, p.min, p.max));
                write_prompt_with_buffer(tty_out, buffer);
                pos = strlen(buffer);
                end = pos;
            }
        }
    }

    // If we somehow exit loop without \n
    buffer[end] = '\0';
}

void terminal_deinit(void) {
    if (tty_in) {
        tcsetattr(fileno(tty_in), TCSAFLUSH, &original_termios);
        fclose(tty_in);
        tty_in = NULL;
    }
    if (tty_out) {
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

int main(void) {
    const char *file_path = "test.json";
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

    // Clear console
    fprintf(tty_out, CLR HOME);

    // Display form title in bold
    fprintf(tty_out, BOLD"%s"RESET"\r\n", form.title);

    Option option = pretty;

    Answers answers = {0};
    nob_da_reserve(&answers, form.fields.count);

    char answer_buffer[BUFFER_LEN];
    char quoted_answer_buffer[BUFFER_LEN+2];
    const char *timestamp_field_id = NULL;
    nob_da_foreach(Field, f, &form.fields) {
        switch (f->type) {
        case ft_text: {
            write_question(tty_out, f->text);

            do {
                read_input(answer_buffer, f);
            } while (fails_checks(f, answer_buffer));

            write_nl(tty_out);

            if (is_empty(answer_buffer)) {
                append_answer(&answers, f->id, "null");
            }
            else {
                sprintf(quoted_answer_buffer, "\"%s\"", answer_buffer);
                append_answer(&answers, f->id, quoted_answer_buffer);
            }
        } break;

        case ft_number: {
            write_question(tty_out, f->number);

            do {
                read_input(answer_buffer, f);
            } while (fails_checks(f, answer_buffer));

            write_nl(tty_out);

            append_answer(&answers, f->id,
                is_empty(answer_buffer) ? "null" : answer_buffer);
        } break;

        case ft_select: {
            write_question(tty_out, f->select);
            bool first_time = true;
            do {
                read_select(answer_buffer, f, first_time);
                first_time = false;
            } while (fails_checks(f, answer_buffer));

            if (is_empty(answer_buffer)) {
                append_answer(&answers, f->id, "null");
            }
            else {
                sprintf(quoted_answer_buffer, "\"%s\"", answer_buffer);
                append_answer(&answers, f->id, quoted_answer_buffer);
            }
        } break;

        case ft_multiselect: {
            MultiSelectFieldMembers p = f->multiselect;
            fprintf(tty_out, "%s ", p.question);
            if (p.max == UINT_MAX) {
                if (p.min == 0) {
                    fprintf(tty_out, "(any)\r\n");
                }
                else {
                    fprintf(tty_out, "(at least %d)\r\n", p.min);
                }
            }
            else {
                fprintf(tty_out, "(%d-%d)\r\n", p.min, p.max);
            }

            bool first_time = true;
            SelectOptions opts = {0};
            bool selected_indexes[100] = {0};
            do {
                opts.count = 0;
                read_multiselect(selected_indexes, &opts, f, first_time);
                first_time = false;
            } while (fails_multiselect_checks(f, opts.count));

            append_multiselect_answer(&answers, f->id, &opts);
        } break;

        case ft_counter: {
            fprintf(tty_out, "%s\r\n", f->counter.question);
            fflush(tty_out);

            uint64_t value = read_counter();

            write_nl(tty_out);

            sprintf(answer_buffer, "%lu", value);
            append_answer(&answers, f->id, answer_buffer);
        } break;

        case ft_color: {
            fprintf(tty_out, "%s\r\n", f->color.question);
            fflush(tty_out);
            Color choice = read_color();
            write_nl(tty_out);

            color_to_str(answer_buffer, choice);
            sprintf(quoted_answer_buffer, "\"%s\"", answer_buffer);
            append_answer(&answers, f->id, quoted_answer_buffer);
        } break;

        case ft_bool: {
            fprintf(tty_out, "%s\r\n", f->boolean.question);
            fflush(tty_out);
            bool choice = read_bool();
            append_answer(&answers, f->id, choice ? "true" : "false");
        } break;

        case ft_timestamp:
            timestamp_field_id = f->id;
            break;

        default: NOB_UNREACHABLE("Unidentified type!");
        }
    }

    if (timestamp_field_id) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        strftime(quoted_answer_buffer, sizeof(quoted_answer_buffer), "\"%Y-%m-%d %H:%M:%S\"", t);
        append_answer(&answers, timestamp_field_id, quoted_answer_buffer);
    }

    terminal_deinit();

    // This necessary to output json double correctly
    setlocale(LC_NUMERIC, "C");

    Jim jim = {.pp = option == pretty ? 4 : 0};
    jim_answers(&jim, &answers);
    fwrite(jim.sink, jim.sink_count, 1, stdout);

    return 0;
}
