#include "form_cli.h"

#include <inttypes.h>
#include <pthread.h>
#include <unistd.h>

#define NANOSEC_PER_SEC 1000000000ULL
#define NANOSEC_PER_MIN (60 * NANOSEC_PER_SEC)
#define NANOSEC_PER_HOUR (60 * NANOSEC_PER_MIN)
#define NANOSEC_PER_CENTISEC 10000000ULL
#define TIMER_SLEEP_TIME 10000

static volatile bool running = false;
static volatile uint64_t nanoseconds_total = 0;

static void fprint_timer(FILE *stream, uint64_t total) {
    uint64_t hours = total / NANOSEC_PER_HOUR;
    uint64_t minutes = (total % NANOSEC_PER_HOUR) / NANOSEC_PER_MIN;
    uint64_t seconds = (total % NANOSEC_PER_MIN) / NANOSEC_PER_SEC;
    uint64_t centi = (total % NANOSEC_PER_SEC) / NANOSEC_PER_CENTISEC;

    fprintf(stream, "\r%02lld:%02lld:%02lld:%02lld"CLRDOWN, hours, minutes, seconds, centi);
    fflush(stream);
}

static void *timer_thread(void *arg) {
    NOB_UNUSED(arg);

    running = true;
    struct timespec last;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &last);
    while (running) {
        clock_gettime(CLOCK_MONOTONIC, &now);

        uint64_t elapsed_ns = (uint64_t) (now.tv_sec - last.tv_sec) * NANOSEC_PER_SEC
            + (uint64_t) (now.tv_nsec - last.tv_nsec);

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
                running = false;

                fprintf(tty_out, UP(1)"\rTimer paused. Press [space] to start, [esc] to reset, or [enter] to submit."CLRDOWN"\r\n");
                fprint_timer(tty_out, nanoseconds_total);

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
            if (pthread_create(&timer_tid, NULL, timer_thread, NULL) != 0) {
                perror("Failed to create timer thread");
                return (uint64_t) -1;
            }
        }
    }
}

int ns_to_iso8601_duration(uint64_t total_ns, char *buf, size_t bufsize) {
    NOB_ASSERT(bufsize > 32);
    uint64_t hours = total_ns / NANOSEC_PER_HOUR;
    uint64_t minutes = (total_ns % NANOSEC_PER_HOUR) / NANOSEC_PER_MIN;
    uint64_t seconds = (total_ns % NANOSEC_PER_MIN) / NANOSEC_PER_SEC;
    uint64_t centi = (total_ns % NANOSEC_PER_SEC) / NANOSEC_PER_CENTISEC;

    char *p = buf;
    char *end = buf + bufsize;

    *p++ = 'P';
    *p++ = 'T';

    bool wrote_something = false;

    if (hours > 0) {
        int n = snprintf(p, (size_t) (end - p), "%" PRIu64 "H", hours);
        if (n <= 0) return -1;
        p += n;
        wrote_something = true;
    }

    if (minutes > 0) {
        int n = snprintf(p, (size_t) (end - p), "%" PRIu64 "M", minutes);
        if (n <= 0) return -1;
        p += n;
        wrote_something = true;
    }

    if (seconds > 0 || centi > 0 || !wrote_something) {
        if (centi > 0) {
            int n = snprintf(p, (size_t) (end - p), "%" PRIu64 ".%02" PRIu64 "S", seconds, centi);
            if (n <= 0) return -1;
            p += n;
        }
        else {
            int n = snprintf(p, (size_t) (end - p), "%" PRIu64 "S", seconds);
            if (n <= 0) return -1;
            p += n;
        }
        wrote_something = true;
    }

    if (!wrote_something) {
        snprintf(buf, bufsize, "PT0S");
        return 4;
    }

    *p = '\0';
    return (int) (p - buf);
}
