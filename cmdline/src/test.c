#include "form_app.h"

#include <errno.h>
#include <pthread.h>
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

#define TEST_CHECK(condition, ...) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, __VA_ARGS__); \
            fprintf(stderr, "\n"); \
            return false; \
        } \
    } while (0)

time_t time(time_t *timer) {
    if (timer != NULL) *timer = FIXED_TEST_TIME;
    return FIXED_TEST_TIME;
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
    append_text(steps, "Alice");
    append_char(steps, '\n');

    append_text(steps, "123-45-6789");
    append_char(steps, '\n');

    append_text(steps, "42");
    append_char(steps, '\n');

    append_text(steps, "180.5");
    append_char(steps, '\n');

    append_key(steps, ARROW_DOWN);
    append_char(steps, '\n');

    append_char(steps, ' ');
    append_key(steps, ARROW_DOWN);
    append_key(steps, ARROW_DOWN);
    append_key(steps, ARROW_DOWN);
    append_key(steps, ARROW_DOWN);
    append_char(steps, ' ');
    append_char(steps, '\n');

    append_key(steps, ARROW_UP);
    append_key(steps, ARROW_RIGHT);
    for (int i = 0; i < 15; ++i) append_key(steps, ARROW_UP);
    append_key(steps, ARROW_RIGHT);
    for (int i = 0; i < 26; ++i) append_key(steps, ARROW_DOWN);
    append_char(steps, '\n');

    append_key(steps, ARROW_UP);
    append_key(steps, ARROW_UP);
    append_char(steps, '\n');

    append_key(steps, ARROW_LEFT);
    append_char(steps, '1');
    append_key(steps, ARROW_RIGHT);
    append_char(steps, 'A');
    append_key(steps, ARROW_RIGHT);
    append_char(steps, '2');
    append_key(steps, ARROW_RIGHT);
    append_char(steps, 'B');
    append_key(steps, ARROW_RIGHT);
    append_char(steps, '3');
    append_key(steps, ARROW_RIGHT);
    append_char(steps, 'C');
    append_char(steps, '\n');

    append_key(steps, ARROW_DOWN);
    append_char(steps, '\n');

    append_text(steps, "mario,zelda");
    append_char(steps, '\n');

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
            TEST_CHECK(feof(stream), "failed reading stream");
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
    TEST_CHECK(load_form_from_file(path, &form), "failed to deserialize %s", path);

    size_t counts[FIELD_TYPE_LENGTH] = {0};
    nob_da_foreach(Field, field, &form.fields) {
        TEST_CHECK(field->type < FIELD_TYPE_LENGTH, "field %s has invalid type", field->id);
        counts[field->type]++;
    }

    for (size_t i = 0; i < FIELD_TYPE_LENGTH; ++i) {
        TEST_CHECK(counts[i] > 0, "missing field type index %zu in %s", i, path);
    }

    printf("PASS deserialize %s (%zu fields)\n", path, form.fields.count);
    return true;
}

static bool test_basic_form_script(const char *form_path, const char *answers_path) {
    Form form = {0};
    TEST_CHECK(load_form_from_file(form_path, &form), "failed to deserialize %s", form_path);

    Nob_String_Builder expected = {0};
    TEST_CHECK(nob_read_entire_file(answers_path, &expected), "failed to read %s", answers_path);

    InputSteps steps = {0};
    build_basic_form_script(&steps);

    int input_fds[2];
    TEST_CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, input_fds) == 0,
        "socketpair failed: %s", strerror(errno));

    FILE *input_stream = fdopen(input_fds[0], "r");
    TEST_CHECK(input_stream != NULL, "fdopen failed for scripted input: %s", strerror(errno));

    FILE *terminal_stream = tmpfile();
    TEST_CHECK(terminal_stream != NULL, "tmpfile failed: %s", strerror(errno));

    tty_in = input_stream;
    tty_out = terminal_stream;

    ScriptWriter writer = {
        .fd = input_fds[1],
        .steps = &steps,
    };
    pthread_t writer_thread;
    TEST_CHECK(pthread_create(&writer_thread, NULL, script_writer_main, &writer) == 0,
        "pthread_create failed");

    Answers answers = {0};
    nob_da_reserve(&answers, form.fields.count);
    display_form(&form, &answers);

    TEST_CHECK(pthread_join(writer_thread, NULL) == 0, "pthread_join failed");

    FILE *json_stream = tmpfile();
    TEST_CHECK(json_stream != NULL, "tmpfile failed: %s", strerror(errno));
    output_answers(&answers, 0, json_stream);

    Nob_String_Builder actual = {0};
    TEST_CHECK(read_stream(json_stream, &actual), "failed to read generated answers");
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

    printf("PASS scripted %s\n", form_path);
    return true;
}

int main(void) {
    const char deserialize_path[] = "comprehensive_test_form.json";
    const char scripted_path[] = "basic_form.json";
    const char answers_path[] = "basic_form_answers.json";

    setenv("TZ", "UTC", 1);
    tzset();

    if (!test_deserialize_all_fields(deserialize_path)) return 1;
    if (!test_basic_form_script(scripted_path, answers_path)) return 1;

    puts("All tests passed.");
    return 0;
}
