#define JIM_IMPLEMENTATION
#define JIMP_IMPLEMENTATION
#define NOB_IMPLEMENTATION
#include "jim_form.h"

#include <termios.h>
#include <locale.h>

#define ESC "\033"
#define CLR    ESC"[2J"
#define HOME   ESC"[H"
#define RESET  ESC"[0m"
#define BOLD   ESC"[1m"
#define FAINT  ESC"[2m"
#define UP     ESC"[1A"
#define DOWN   ESC"[1B"
#define CLRDOWN ESC"[K"

#define BUFFER_LEN 1024

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CLAMP(x, lower, upper) (MAX((lower), MIN((upper), (x))))
#define IS_BETWEEN(x, lower, upper) ((x) >= (lower) && (x) <= (upper))

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
            bool between = isnum && IS_BETWEEN(to_double(answer), p1.min, p1.max);
            return between && !isnum && !req || isnum && !between && !req || !empty && !isnum && !req;

        default:
            assert("Unidentified type!\n");
    }
}

static struct termios original_termios;
static FILE *tty_in = NULL;
static FILE *tty_out = NULL;

void read_number(char *buffer, double min, double max, double step) {
    fflush(tty_out);

    size_t pos = 0;
    buffer[0] = '\0';

    while (1) {
        // Read one raw byte
        unsigned char c;
        if (read(fileno(tty_in), &c, 1) != 1) {
            break; // error or EOF
        }

        switch (c) {
            case '\n':          // Enter pressed
            case '\r':          // Sometimes terminals send \r
                putc('\r', tty_out);
                putc('\n', tty_out);
                fflush(tty_out);
                buffer[pos] = '\0';
                return;

            case 127:           // Backspace (most common)
            case '\b':          // Some terminals send \b
                if (pos > 0) {
                    buffer[--pos] = '\0';
                    // Move cursor left, overwrite with space, move left again
                    fprintf(tty_out, "\b \b");
                    fflush(tty_out);
                }
                break;

            case 3:             // Ctrl+C
            case 4:             // Ctrl+D
                exit(EXIT_FAILURE);

            // up and down
            case '\033':          // Escape sequence start
                if (read(fileno(tty_in), &c, 1) != 1 && c != '[')  break; // error or EOF
                if (read(fileno(tty_in), &c, 1) != 1)  break; // error or EOF

                double current = round(to_double(buffer) / step) * step;
                double new_value = current;
                if (c == 'A') { // Up arrow
                    new_value += step;
                }
                else if (c == 'B') { // Down arrow
                    new_value -= step;
                }
                else break;
                snprintf(buffer, BUFFER_LEN, "%g", CLAMP(new_value, min, max));
                fprintf(tty_out, "\r> %s" CLRDOWN, buffer);
                fflush(tty_out);
                pos = strlen(buffer);
                break;

            default:
                // Numeric character or '-' or '.'
                if (c == '-' || c == '.' || isdigit(c)) {
                    if (pos < BUFFER_LEN - 1) {
                        buffer[pos++] = (char)c;
                        putc(c, tty_out);
                        fflush(tty_out);
                    }
                }
                break;
        }
    }

    // If we somehow exit loop without \n
    buffer[pos] = '\0';
}

void read_string(char *buffer, size_t maxlen) {
    fflush(tty_out);
    if (maxlen == 0) {
        buffer[0] = '\0';
        return;
    }

    size_t pos = 0;
    buffer[0] = '\0';

    while (1) {
        // Read one raw byte
        unsigned char c;
        if (read(fileno(tty_in), &c, 1) != 1) {
            break; // error or EOF
        }

        switch (c) {
            case '\n':          // Enter pressed
            case '\r':          // Sometimes terminals send \r
                putc('\r', tty_out);
                putc('\n', tty_out);
                fflush(tty_out);
                buffer[pos] = '\0';
                return;

            case 127:           // Backspace (most common)
            case '\b':          // Some terminals send \b
                if (pos > 0) {
                    pos--;
                    // Move cursor left, overwrite with space, move left again
                    fprintf(tty_out, "\b \b");
                    fflush(tty_out);
                }
                break;

            case 3:             // Ctrl+C
            case 4:             // Ctrl+D
                exit(EXIT_FAILURE);

            default:
                // Printable character
                if (c >= 32 && c <= 126) {  // basic printable ASCII
                    if (pos < maxlen - 1) {
                        buffer[pos++] = (char)c;
                        putc(c, tty_out);
                        fflush(tty_out);
                    }
                    // else: silently ignore (max length reached)
                }
                break;
        }
    }

    buffer[pos] = '\0';
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
            TextFieldMembers p0 = f->text;
            fprintf(tty_out, "%s%s\r\n> ", p0.question, p0.required ? "*" : "");
            fprintf(tty_out, FAINT"%s"RESET"\r\n\r\n", p0.placeholder);

            do {
                fprintf(tty_out, UP CLRDOWN "> ");
                read_string(answer_buffer, p0.maxlength);
            } while (fails_checks(f, answer_buffer));

            if (is_empty(answer_buffer)) {
                append_answer(&answers, f->id, "null");
            }
            else {
                sprintf(quoted_answer_buffer, "\"%s\"", answer_buffer);
                append_answer(&answers, f->id, quoted_answer_buffer);
            }
            break;

        case ft_number:
            NumberFieldMembers p1 = f->number;
            fprintf(tty_out, "%s%s\r\n\r\n", p1.question, p1.required ? "*" : "");

            do {
                fprintf(tty_out, UP CLRDOWN "> ");
                read_number(answer_buffer, p1.min, p1.max, p1.step);
            } while (fails_checks(f, answer_buffer));

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
