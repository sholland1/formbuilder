#include "form_app.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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

static void on_script_timeout(int signo) {
    (void) signo;
    static const char message[] =
        FAIL"scripted test timed out after " STRINGIFY(SCRIPT_TIMEOUT_SECONDS)
        " second(s). Input script likely no longer matches prompts.\n";
    write(STDERR_FILENO, message, sizeof(message) - 1);
    _exit(124);
}

static void append_step(InputSteps *steps, const char *bytes, size_t len) {
    InputStep step = {0};
    step.bytes = (char *) malloc(len);
    NOB_ASSERT(step.bytes != NULL);
    memcpy(step.bytes, bytes, len);
    step.len = len;
    nob_da_append(steps, step);
}

static void append_char(InputSteps *steps, char ch) {
    append_step(steps, &ch, 1);
}

static void append_text(InputSteps *steps, const char *text) {
    for (const char *p = text; *p != '\0'; ++p) {
        append_char(steps, *p);
    }
}

static void append_key(InputSteps *steps, const char *sequence) {
    append_step(steps, sequence, strlen(sequence));
}

#define ARROW_UP "\033[A"
#define ARROW_DOWN "\033[B"
#define ARROW_RIGHT "\033[C"
#define ARROW_LEFT "\033[D"

static void build_basic_form_script(InputSteps *steps) {
    // text field
    append_char(steps, '\n');
    append_text(steps, "XXXX");
    append_char(steps, '\033');
    append_text(steps, "Alice");
    append_char(steps, '\n');

    // text field with pattern
    append_text(steps, "XXXX");
    append_char(steps, '\n');
    for (int i = 0; i < 4; i++) append_char(steps, '\b');
    append_text(steps, "123-45-6789");
    append_char(steps, '\n');

    // number field with step of 1
    append_text(steps, "12");
    for (int i = 0; i < 31; ++i) append_key(steps, ARROW_UP);
    append_key(steps, ARROW_DOWN);
    append_char(steps, '\n');

    // number field with step of .1
    append_text(steps, "180.4");
    append_key(steps, ARROW_UP);
    append_char(steps, '\n');

    // select field
    append_key(steps, ARROW_DOWN);
    append_char(steps, '\n');

    // multiselect field
    append_char(steps, ' ');
    append_key(steps, ARROW_DOWN);
    append_key(steps, ARROW_DOWN);
    append_key(steps, ARROW_DOWN);
    append_key(steps, ARROW_DOWN);
    append_char(steps, ' ');
    append_char(steps, '\n');

    // date field
    append_key(steps, ARROW_UP);
    append_key(steps, ARROW_RIGHT);
    for (int i = 0; i < 15; ++i) append_key(steps, ARROW_UP);
    append_key(steps, ARROW_RIGHT);
    for (int i = 0; i < 26; ++i) append_key(steps, ARROW_DOWN);
    append_char(steps, '\n');

    // counter field
    append_char(steps, ' ');
    append_char(steps, ' ');
    append_key(steps, ARROW_UP);
    append_key(steps, ARROW_DOWN);
    append_char(steps, '\n');

    // color field #1A2B3C
    append_key(steps, ARROW_LEFT);
    append_key(steps, ARROW_UP);
    append_key(steps, ARROW_RIGHT);
    append_char(steps, 'a');
    append_key(steps, ARROW_RIGHT);
    append_key(steps, ARROW_RIGHT);
    for (int i = 0; i < 0x2B; i++) append_key(steps, ARROW_UP);
    append_key(steps, ARROW_RIGHT);
    append_char(steps, '3');
    append_key(steps, ARROW_RIGHT);
    append_char(steps, 'C');
    append_char(steps, '\n');

    // bool field
    append_key(steps, ARROW_DOWN);
    append_char(steps, '\n');

    // multitext field
    append_text(steps, "mario,zelda");
    append_char(steps, '\n');

    // timer field
    append_char(steps, '\n');
}

static void *script_writer_main(void *arg) {
    ScriptWriter *writer = (ScriptWriter *) arg;
    for (size_t i = 0; i < writer->steps->count; ++i) {
        InputStep *step = &writer->steps->items[i];
        ssize_t written = write(writer->fd, step->bytes, step->len);
        if (written < 0) {
            perror("write");
            break;
        }
        if ((size_t) written != step->len) {
            fprintf(stderr, "short write while writing scripted input\n");
            break;
        }
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
        sb->count--;
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

static bool test_basic_form_script(const char *form_path, const char *answers_path) {
    Form form = {0};
    TEST_CHECK(load_form_from_file(form_path, &form), "deserialize %s", form_path);
    TEST_CHECK(signal(SIGALRM, on_script_timeout) != SIG_ERR, "install timeout handler");

    Nob_String_Builder expected = {0};
    TEST_CHECK(nob_read_entire_file(answers_path, &expected), "read %s", answers_path);

    InputSteps steps = {0};
    build_basic_form_script(&steps);

    int input_fds[2];
    TEST_CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, input_fds) == 0,
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
    output_answers(&answers, 0, json_stream);

    Nob_String_Builder actual = {0};
    TEST_CHECK(read_stream(json_stream, &actual), "read generated answers");
    trim_trailing_newlines(&expected);
    trim_trailing_newlines(&actual);
    TEST_CHECK(actual.count == expected.count,
        "answer length mismatch: expected %zu, got %zu", expected.count, actual.count);
    TEST_CHECK(memcmp(actual.items, expected.items, expected.count) == 0,
        "generated answers do not match %s", answers_path);

    fclose(json_stream);
    fclose(terminal_stream);
    fclose(input_stream);
    tty_in = NULL;
    tty_out = NULL;
    free_steps(&steps);

    printf(PASS"scripted %s\n", form_path);
    return true;
}

int main(void) {
    const char deserialize_path[] = "comprehensive-test-form.json";
    const char scripted_path[] = "basic-form.json";
    const char answers_path[] = "basic-form.answers.json";

    setenv("TZ", "UTC", 1);
    tzset();

    if (!test_deserialize_all_fields(deserialize_path)) return 1;
    if (!test_basic_form_script(scripted_path, answers_path)) return 1;

    puts(PASS"All tests passed.");
    return 0;
}
