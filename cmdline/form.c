#define JIM_IMPLEMENTATION
#define JIMP_IMPLEMENTATION
#define NOB_IMPLEMENTATION
#include "jim_form.h"

#include <termios.h>
#include <locale.h>

#define _STR(x)  #x
#define __STR(x) _STR(x)

#define ESC "\033"
#define CLR      ESC"[2J"
#define HOME     ESC"[H"
#define RESET    ESC"[0m"
#define BOLD     ESC"[1m"
#define FAINT    ESC"[2m"
#define RED      ESC"[31m"
#define CLRDOWN  ESC"[K"
#define RIGHT(n) ESC"["__STR(n)"G"

#define BUFFER_LEN 1024

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CLAMP(x, lower, upper) (MAX((lower), MIN((upper), (x))))
#define BETWEEN(x, lower, upper) ((x) >= (lower) && (x) <= (upper))

void append_answer(Answers *answers, const char *id, const char *value) {
    // LEAK: Memory must be used, and it'll be cleaned up at exit
    Answer a = {
        .id = strdup(id),
        .value = strdup(value),
    };
    da_append(answers, a);
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
        if (isdigit((unsigned char)*str)) {
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
    return !*s;
}

double to_double(const char* str) {
    return str ? strtod(str, NULL) : 0.0;
}

bool fails_checks(const Field *f, const char *answer) {
    bool empty = is_empty(answer);
    switch (f->type) {
        case ft_text:
            TextFieldMembers p0 = f->text;
            return empty
                ? p0.required
                : !is_match(p0.regex, answer);
            break;

        case ft_number:
            NumberFieldMembers p1 = f->number;
            bool req = p1.required;
            bool isnum = !empty && is_numeric(answer);
            bool between = isnum && BETWEEN(to_double(answer), p1.min, p1.max);
            return (between && !isnum && !req) || (isnum && !between && !req) || (!empty && !isnum && !req);

        default: break;
    }
    assert("Unidentified type!\n");
    return false;
}

static struct termios original_termios;
static FILE *tty_in = NULL;
static FILE *tty_out = NULL;

void make_prompt_red(size_t pos, bool b) {
    // move to beginning of line
    putc('\r', tty_out);
    // put red or normal '>'
    fprintf(tty_out, b ? RED"> "RESET : "> ");
    // move back to original position
    fprintf(tty_out, RIGHT(%zu), pos+3);
    fflush(tty_out);
}

typedef enum {
    key_eof,
    key_char,
    key_delete,
    key_ctrl_delete,
    key_backspace,
    key_ctrl_backspace,
    key_enter,
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
    unsigned char ch;
} Key;

#define KEY(x) (Key){.type = (x)}
#define KEY_EOF KEY(key_eof)

Key read_key(FILE *stream) {
    unsigned char c;
    if (read(fileno(stream), &c, 1) != 1) return KEY_EOF;

    if (c == 3 || c == 4) return KEY(key_exit); // Ctrl+C or Ctrl+D
    if (c == '\n' || c == '\r')  return KEY(key_enter);
    if (c >= 32 && c <= 126) return  (Key){.type = key_char, .ch = (c)}; // Printable ASCII
    if (c == 127 || c == '\b') return KEY(key_backspace);
    if (c == 23) return KEY(key_ctrl_backspace);

    if (c == '\033') { // Escape sequence
        if (read(fileno(stream), &c, 1) != 1 && c != '[') return KEY_EOF;
        if (c == 'd') return KEY(key_ctrl_delete);

        if (read(fileno(stream), &c, 1) != 1 && c != '[') return KEY_EOF;

        if (c == 'A') return KEY(key_arrow_up);
        if (c == 'B') return KEY(key_arrow_down);
        if (c == 'C') return KEY(key_arrow_right);
        if (c == 'D') return KEY(key_arrow_left);
        if (c == 'F') return KEY(key_end);
        if (c == 'H') return KEY(key_home);
        if (c == '1') {
            if (read(fileno(stream), &c, 1) != 1 && c != ';') return KEY_EOF;
            if (read(fileno(stream), &c, 1) != 1 && c != '5') return KEY_EOF;
            if (read(fileno(stream), &c, 1) != 1) return KEY_EOF;

            if (c == 'C') return KEY(key_ctrl_right);
            if (c == 'D') return KEY(key_ctrl_left);
        }
        if (c == '3') {
            if (read(fileno(stream), &c, 1) != 1 && c != '~') return KEY_EOF;
            return KEY(key_delete);
        }
    }

    return KEY(key_unknown);
}

bool is_word_boundary(char c) {
    return c == ' ' || c == '.';
}

void write_prompt_with_buffer(FILE *stream, const char *buffer) {
    fprintf(stream, "\r> %s" CLRDOWN, buffer);
    fflush(stream);
}

void write_placeholder(FILE *stream, const char *ph) {
    fprintf(stream, "\r> "FAINT"%s"RESET"\r"RIGHT(3), ph);
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
        make_prompt_red(pos, fails_checks(field, buffer));

        Key k = read_key(tty_in);
        if (k.type == key_exit) { // Ctrl+C or Ctrl+D
            exit(EXIT_FAILURE);
        }
        if (k.type == key_enter) {
            buffer[end] = '\0';
            return;
        }

        if (k.type == key_backspace) { // Backspace
            if (pos > 0) {
                for (int i = pos; i < end; i++) {
                    buffer[i-1] = buffer[i];
                }
                pos--;
                buffer[--end] = '\0';
                write_prompt_with_buffer(tty_out, buffer);
            }
        }
        else if (k.type == key_ctrl_backspace) { // Ctrl+Backspace
            int orig_pos = pos;
            while (pos > 0 && is_word_boundary(buffer[pos-1])) pos--;
            while (pos > 0 && !is_word_boundary(buffer[pos-1])) pos--;
            int del_len = orig_pos-pos;
            for (int i = orig_pos; i < end; i++) {
                buffer[i-del_len] = buffer[i];
            }
            end -= del_len;
            buffer[end] = '\0';
            write_prompt_with_buffer(tty_out, buffer);
        }
        else if (k.type == key_delete) {
            if (pos < end) {
                for (int i = pos + 1; i < end; i++) {
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
            for (int i = pos; i < end; i++) {
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
                    for (int i = end+1; i >= pos+1; i--) {
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
            NumberFieldMembers p0 = field->number;
            if (k.type == key_char) {
                if (k.ch == '-' || k.ch == '.' || isdigit(k.ch)) {
                    // if (pos < BUFFER_LEN - 1) {
                    if (pos < end) {
                        for (int i = end+1; i >= pos+1; i--) {
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
                if (k.type == key_arrow_up) { // Up arrow
                    signed_step = p0.step;
                }
                else if (k.type == key_arrow_down) { // Down arrow
                    signed_step = -p0.step;
                }
                else break;
                double current = round(to_double(buffer) / p0.step) * p0.step;
                snprintf(buffer, BUFFER_LEN, "%g", CLAMP(current + signed_step, p0.min, p0.max));
                write_prompt_with_buffer(tty_out, buffer);
                pos = strlen(buffer);
                end = pos;
            }
        }
    }

    // If we somehow exit loop without \n
    buffer[end] = '\0';
}

void deinit(void) {
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

void init(void) {
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
        deinit();           // best effort cleanup
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
        deinit();
        exit(EXIT_FAILURE);
    }

    // Register normal exit cleanup
    if (atexit(deinit) != 0) {
        fprintf(stderr, "atexit failed\n");
        deinit();
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    const char *file_path = "../test.json";
    String_Builder sb = {0};
    if (!read_entire_file(file_path, &sb)) return 1;

    Jimp jimp = {0};
    jimp_begin(&jimp, file_path, sb.items, sb.count);

    Form form = {0};
    if (!jimp_form(&jimp, &form)) {
        nob_log(ERROR, "Failed to parse form in %s", file_path);
        return 1;
    }

    init();

    // Clear console
    fprintf(tty_out, CLR HOME);

    // Display form title in bold
    fprintf(tty_out, BOLD"%s"RESET"\r\n", form.title);

    Option option = pretty;

    Answers answers = {0};

    char answer_buffer[BUFFER_LEN];
    char quoted_answer_buffer[BUFFER_LEN+2];
    da_foreach(Field, f, &form.fields) {
        switch (f->type) {
        case ft_text:
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
            break;

        case ft_number:
            write_question(tty_out, f->number);

            do {
                read_input(answer_buffer, f);
            } while (fails_checks(f, answer_buffer));

            write_nl(tty_out);

            append_answer(&answers, f->id,
                is_empty(answer_buffer) ? "null" : answer_buffer);
            break;

        default:
            assert("Unidentified type!\n");
        }
    }

    deinit();

    // This necessary to output json double correctly
    setlocale(LC_NUMERIC, "C");

    Jim jim = {.pp = option == pretty ? 4 : 0};
    jim_answers(&jim, &answers);
    fwrite(jim.sink, jim.sink_count, 1, stdout);

    return 0;
}
