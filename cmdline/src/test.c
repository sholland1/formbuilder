#include "form_app.h"
#include "types.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <uuid/uuid.h>

typedef struct {
    char *bytes;
    size_t len;
} InputStep;

typedef struct {
    InputStep *items;
    size_t count;
    size_t capacity;
} InputSteps;

typedef struct {
    int fd;
    InputSteps *steps;
} ScriptWriter;

#define FORM_DIR "test_forms/"
typedef struct {
    const char *form_name;
    const char *answer_structure;
    void (*script)(InputSteps*);
} FormTestCase;

AnswerStructureType parse_answer_structure_type(const char *type_str) {
    if (strcmp(type_str, "basic") == 0) return ast_basic;
    if (strcmp(type_str, "flat") == 0) return ast_flat;
    if (strcmp(type_str, "nested") == 0) return ast_nested;
    NOB_UNREACHABLE("Unidentified type!");
}

#define FIXED_TEST_TIME ((time_t)1772368496)
#define SCRIPT_TIMEOUT_SECONDS 1
#define STRINGIFY_(value) #value
#define STRINGIFY(value) STRINGIFY_(value)

#define PASS GREEN"✔ "RESET
#define FAIL RED"✘ "RESET

#define TEST_CHECK(condition, ...) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, FAIL __VA_ARGS__); \
            fprintf(stderr, "\n"); \
            return false; \
        } \
    } while (0)

time_t time(time_t *timer) {
    if (timer != NULL) *timer = FIXED_TEST_TIME;
    return FIXED_TEST_TIME;
}

void uuid_generate(uuid_t out) {
    static const uint8_t fixed_uuid[16] = {
        0x5b, 0x01, 0xf0, 0x62,
        0xb8, 0x83,
        0x45, 0xff,
        0xa9, 0x34,
        0x2b, 0x69, 0x29, 0x3c, 0x0b, 0xbe,
    };
    memcpy(out, fixed_uuid, sizeof(fixed_uuid));
}

static void on_script_timeout(int signo) {
    (void) signo;
    static const char message[] =
        FAIL"scripted test timed out after " STRINGIFY(SCRIPT_TIMEOUT_SECONDS)
        " second(s). Input script likely no longer matches prompts.\n";
    write(STDERR_FILENO, message, sizeof(message) - 1);
    _exit(124);
}

static void append_step(InputSteps *steps, const char *bytes, size_t len) {
    char *copy = (char *) malloc(len);
    NOB_ASSERT(copy != NULL);
    memcpy(copy, bytes, len);
    nob_da_append(steps, ((InputStep){ copy, len }));
}

#undef UP
#undef RIGHT

#define UP UP_(1)
#define DOWN DOWN_(1)
#define RIGHT RIGHT_(1)
#define LEFT LEFT_(1)
#define UP_(n) KEYS("\033[A", (n))
#define DOWN_(n) KEYS("\033[B", (n))
#define RIGHT_(n) KEYS("\033[C", (n))
#define LEFT_(n) KEYS("\033[D", (n))

#define SPACE(n) CHARS(' ', (n))

// Script DSL for readable test input sequences.
#define SP CHAR(' ')
#define NL CHAR('\n')
#define ESC CHAR('\033')
#define CHAR(ch) append_step(steps, (const char[]){(ch)}, 1)
#define CHARS(ch, n) do { \
    NOB_ASSERT((n) > 0); \
    for (int i = 0; i < (n); i++) CHAR(ch); \
} while (0)
#define TEXT(str) for (const char *p = (str); *p != '\0'; ++p) CHAR(*p)
#define KEYS(seq, n) do { \
    NOB_ASSERT((n) > 0); \
    for (int i = 0; i < (n); i++) append_step(steps, (seq), strlen(seq)); \
} while (0)

static void build_basic_group_form_script(InputSteps *steps) {
    SPACE(5); NL;
    SPACE(6); NL;
    SPACE(7); NL;
}

static void build_basic_form_script(InputSteps *steps) {
    // text field
    NL;
    CHARS('X', 4);
    ESC;
    TEXT("Alice\n");

    // text field with pattern
    CHARS('X', 4); NL;
    CHARS('\b', 4);
    TEXT("123-45-6789\n");

    // number field with step of 1
    TEXT("12");
    UP_(31); DOWN;
    NL;

    // number field with step of .1
    TEXT("180.4"); UP; NL;

    // select field
    DOWN; NL;

    // multiselect field
    SP; DOWN_(4); SP; NL;

    // date field
    UP; RIGHT; UP_(15); RIGHT; DOWN_(26);
    NL;

    // counter field
    SP; SP; UP; DOWN; NL;

    // color field #1A2B3C
    LEFT; UP; RIGHT;
    CHAR('a'); RIGHT_(2);
    UP_(0x2B); RIGHT;
    CHAR('3'); RIGHT;
    CHAR('C');
    NL;

    // bool field
    DOWN; NL;

    // multitext field
    TEXT("mario,zelda\n");

    // timer field
    NL;

    // rating field
    RIGHT_(2);
    ESC;
    LEFT; UP_(2); RIGHT; DOWN; LEFT;
    NL;
}

static void *script_writer_main(void *arg) {
    ScriptWriter *writer = (ScriptWriter *) arg;
    for (size_t i = 0; i < writer->steps->count; ++i) {
        InputStep *step = &writer->steps->items[i];
#if defined(__linux__)
        ssize_t written = write(writer->fd, step->bytes, step->len);
        if (written < 0) {
            perror("write");
            break;
        }
        if ((size_t) written != step->len) {
            fprintf(stderr, "short write while writing scripted input\n");
            break;
        }
#else
        for (;;) {
            ssize_t written = write(writer->fd, step->bytes, step->len);
            if ((size_t) written == step->len) {
                break;
            }
            if (written < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS)) {
                usleep(500);
                continue;
            }
            if (written < 0) {
                perror("write");
                close(writer->fd);
                return NULL;
            }

            fprintf(stderr, "short write while writing scripted input\n");
            close(writer->fd);
            return NULL;
        }
#endif
    }
    close(writer->fd);
    return NULL;
}

static void free_steps(InputSteps *steps) {
    for (size_t i = 0; i < steps->count; ++i) {
        free(steps->items[i].bytes);
    }
    free(steps->items);
    steps->items = NULL;
    steps->count = 0;
    steps->capacity = 0;
}

static bool read_stream(FILE *stream, Nob_String_Builder *sb) {
    char buffer[1024];
    rewind(stream);
    for (;;) {
        size_t count = fread(buffer, 1, sizeof(buffer), stream);
        if (count > 0) {
            nob_sb_append_buf(sb, buffer, count);
        }
        if (count < sizeof(buffer)) {
            TEST_CHECK(feof(stream), "reading stream");
            break;
        }
    }
    return true;
}

static void trim_trailing_newlines(Nob_String_Builder *sb) {
    while (sb->count > 0) {
        char c = sb->items[sb->count - 1];
        if (c != '\n' && c != '\r') break;
        nob_da_pop(sb);
    }
}

static bool test_deserialize_all_fields(const char *path) {
    Form form = {0};
    TEST_CHECK(load_form_from_file(path, &form), "deserialize %s", path);

    size_t counts[FIELD_TYPE_LENGTH] = {0};
    nob_da_foreach(Field, field, &form.fields) {
        TEST_CHECK(field->type < FIELD_TYPE_LENGTH, "field %s has invalid type", field->id);
        counts[field->type]++;
    }

    for (size_t i = 0; i < FIELD_TYPE_LENGTH; ++i) {
        TEST_CHECK(counts[i] > 0, "missing field type index %zu in %s", i, path);
    }

    printf(PASS"deserialize %s (%zu fields)\n", path, form.fields.count);
    return true;
}

static bool test_form_script(const FormTestCase *test_case) {
    char form_path[128];
    snprintf(form_path, 128, FORM_DIR"%s.json", test_case->form_name);
    char answers_path[128];
    if (test_case->answer_structure == NULL) {
        snprintf(answers_path, 128, FORM_DIR"%s.answers.json", test_case->form_name);
    }
    else {
        snprintf(answers_path, 128, FORM_DIR"%s-%s.answers.json", test_case->form_name, test_case->answer_structure);
    }

    Form form = {0};
    TEST_CHECK(load_form_from_file(form_path, &form), "deserialize %s", form_path);
    TEST_CHECK(signal(SIGALRM, on_script_timeout) != SIG_ERR, "install timeout handler");

    Nob_String_Builder expected = {0};
    TEST_CHECK(nob_read_entire_file(answers_path, &expected), "read %s", answers_path);

    InputSteps steps = {0};
    test_case->script(&steps);

    int input_fds[2];
#if defined(__linux__)
    int input_socket_type = SOCK_SEQPACKET;
#else
    int input_socket_type = SOCK_DGRAM;
#endif
    TEST_CHECK(socketpair(AF_UNIX, input_socket_type, 0, input_fds) == 0,
        "socketpair: %s", strerror(errno));

    FILE *input_stream = fdopen(input_fds[0], "r");
    TEST_CHECK(input_stream != NULL, "fdopen for scripted input: %s", strerror(errno));

    FILE *terminal_stream = tmpfile();
    TEST_CHECK(terminal_stream != NULL, "tmpfile: %s", strerror(errno));

    tty_in = input_stream;
    tty_out = terminal_stream;

    ScriptWriter writer = {
        .fd = input_fds[1],
        .steps = &steps,
    };
    pthread_t writer_thread;
    TEST_CHECK(pthread_create(&writer_thread, NULL, script_writer_main, &writer) == 0,
        "pthread_create");

    Answers answers = {0};
    nob_da_reserve(&answers, form.fields.count);
    alarm(SCRIPT_TIMEOUT_SECONDS);
    display_form(&form, &answers);
    alarm(0);

    TEST_CHECK(pthread_join(writer_thread, NULL) == 0, "pthread_join");

    FILE *json_stream = tmpfile();
    TEST_CHECK(json_stream != NULL, "tmpfile: %s", strerror(errno));

    AnswerStructureType answer_structure_type = ast_nested;
    if (test_case->answer_structure != NULL) {
        answer_structure_type = parse_answer_structure_type(test_case->answer_structure);
    }
    output_answers(&answers, 0, answer_structure_type, json_stream);

    Nob_String_Builder actual = {0};
    TEST_CHECK(read_stream(json_stream, &actual), "read generated answers");
    trim_trailing_newlines(&expected);
    trim_trailing_newlines(&actual);

    TEST_CHECK(
        actual.count == expected.count && memcmp(actual.items, expected.items, expected.count) == 0,
        "generated answers do not match:\nexpected: %.*s\nactual:   %.*s",
        (int) expected.count, expected.items,
        (int) actual.count, actual.items);

    fclose(json_stream);
    fclose(terminal_stream);
    fclose(input_stream);
    tty_in = NULL;
    tty_out = NULL;
    free_steps(&steps);

    printf(PASS"scripted %s, type: %s\n", form_path, test_case->answer_structure);
    return true;
}

FormTestCase test_forms[] = {
    {"basic-form", NULL, build_basic_form_script},
    {"basic-group-form", "nested", build_basic_group_form_script},
    {"basic-group-form", "flat", build_basic_group_form_script},
    {"basic-group-form", "basic", build_basic_group_form_script},
};

int main(void) {
    const char deserialize_path[] = FORM_DIR"comprehensive-test-form.json";

    setenv("TZ", "UTC", 1);
    tzset();

    if (!test_deserialize_all_fields(deserialize_path)) return 1;

    for (int i = 0; i < 4; i++) {
        FormTestCase test_case = test_forms[i];
        if (!test_form_script(&test_case)) return 1;
    }

    puts(PASS"All tests passed.");
    return 0;
}
