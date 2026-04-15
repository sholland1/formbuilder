#include "form_cli.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DATE_BUFFER_LEN 11

static bool is_numeric(const char *str) {
    if (str == NULL || *str == '\0') return false;

    if (*str == '+' || *str == '-') str++;

    bool has_dot = false;
    bool has_digit = false;

    while (*str) {
        if (isdigit((uint8_t) *str)) {
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

static double to_double(const char *str) {
    return str ? strtod(str, NULL) : 0.0;
}

static bool fails_multiselect_checks(const Field *f, int opts_count) {
    NOB_ASSERT(f->type == ft_multiselect);
    MultiSelectFieldMembers p = f->multiselect;
    return !BETWEEN(opts_count, p.min, p.max);
}

static bool fails_number_checks(const Field *f, const char *answer) {
    NOB_ASSERT(f->type == ft_number);
    NumberFieldMembers p = f->number;
    bool empty = is_empty(answer);
    bool req = p.required;
    bool isnum = !empty && is_numeric(answer);
    bool between = isnum && BETWEEN(to_double(answer), p.min, p.max);
    return (between && !isnum && !req) || (isnum && !between && !req) || (!empty && !isnum && !req);
}

static bool is_word_boundary(char c) {
    return c == ' ' || c == '.';
}

void color_to_str(char *buffer, Color c) {
    snprintf(buffer, 8, "#%.2X%.2X%.2X", c.r, c.g, c.b);
}

#define RGB_SET(rgb, component, value) \
    (((rgb) & ~(0xFu << (component))) | (((value) & 0xFu) << (component)))

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
            if (pos == 0) c.r += 16;
            else if (pos == 1) c.r++;
            else if (pos == 2) c.g += 16;
            else if (pos == 3) c.g++;
            else if (pos == 4) c.b += 16;
            else if (pos == 5) c.b++;
        }
        else if (k.type == key_arrow_down) {
            if (pos == 0) c.r -= 16;
            else if (pos == 1) c.r--;
            else if (pos == 2) c.g -= 16;
            else if (pos == 3) c.g--;
            else if (pos == 4) c.b -= 16;
            else if (pos == 5) c.b--;
        }
        else if (k.type == key_char && isxdigit(k.ch)) {
            char x = (char) tolower(k.ch);
            x -= isdigit((uint8_t) x) ? '0' : ('a' - 10);
            static const uint8_t pos_to_shift[] = {4, 0, 12, 8, 20, 16};
            c.rgb = RGB_SET(c.rgb, pos_to_shift[pos], x);
        }
    }
}

static void set_default_date(struct tm *result, const date_t *d, const struct tm *current) {
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

static bool is_leap_year(int real_year) {
    if (real_year % 4 != 0) return false;
    if (real_year % 100 != 0) return true;
    return real_year % 400 == 0;
}

static int compare_dates(struct tm t1, struct tm t2) {
    time_t time1 = mktime(&t1);
    time_t time2 = mktime(&t2);

    if (time1 == (time_t) -1 || time2 == (time_t) -1) {
        return 0;
    }

    if (time1 < time2) return -1;
    if (time1 > time2) return 1;
    return 0;
}

static int days_in_month(int month, int real_year) {
    static const int monthly_days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31,
    };
    if (month == 0) return 31;
    return month == 2 && is_leap_year(real_year) ? 29 : monthly_days[month - 1];
}

static bool is_valid_date(int real_year, int month, int day, const struct tm *start, const struct tm *end) {
    if (month > 0 && day > 0
        && BETWEEN(day, 1, days_in_month(month, real_year))
        && BETWEEN(FAKE_YEAR(real_year), start->tm_year, end->tm_year)) {
        struct tm d = {0};
        d.tm_year = FAKE_YEAR(real_year);
        d.tm_mon = month - 1;
        d.tm_mday = day;
        d.tm_hour = 12;
        return compare_dates(d, *start) >= 0 && compare_dates(d, *end) <= 0;
    }
    return false;
}

bool read_date(const Field *f, struct tm *value) {
    NOB_ASSERT(f->type == ft_date);

    static const char *month_names[] = {
        "<Month>",
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December",
    };

    int pos = 0;

    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
    struct tm start_date = {0};
    struct tm end_date = {0};
    set_default_date(&start_date, &f->date.start_date, current_time);
    set_default_date(&end_date, &f->date.end_date, current_time);

    static char buffer[DATE_BUFFER_LEN];
    strftime(buffer, DATE_BUFFER_LEN, "%m/%d/%Y", &start_date);
    fprintf(tty_out, HIDE"%s%s : [%s - ", f->date.question, f->date.required ? "*" : "", buffer);
    strftime(buffer, DATE_BUFFER_LEN, "%m/%d/%Y", &end_date);
    fprintf(tty_out, "%s]\r\n", buffer);
    fflush(tty_out);

    const uint16_t default_year = CLAMP(current_time->tm_year, start_date.tm_year, end_date.tm_year);
    uint16_t year = default_year;
    uint8_t month = 0;
    uint8_t day = 0;

    while (1) {
        bool failed_checks = f->date.required && !is_valid_date(REAL_YEAR(year), month, day, &start_date, &end_date);
        fprintf(tty_out, "\r%s ", failed_checks ? ERR_PROMPT : PROMPT);
        if (pos == 0) fprintf(tty_out, UNDERLINE);
        fprintf(tty_out, pos == 0 ? "%9s" : "%3s", month_names[month]);
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
                value->tm_mon = month - 1;
                value->tm_mday = day;
                value->tm_hour = 12;
                return true;
            }
            if (!f->date.required) {
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

Tristate read_bool(const Field *f) {
    NOB_ASSERT(f->type == ft_bool);

    #define OPTS_LEN 2
    const char *opts[OPTS_LEN] = {"Yes", "No"};
    bool required = f->boolean.required;
    fprintf(tty_out, HIDE"%s%s", f->boolean.question, required ? "*" : "");

    size_t pos = 0;
    while (1) {
        if (!required) {
            fprintf(tty_out, "\r\n%s", pos == 0 ? PROMPT : " ");
        }
        for (size_t i = !required; i < OPTS_LEN + !required; i++) {
            fprintf(tty_out, "\r\n%s %s", pos == i ? PROMPT : " ", opts[i - !required]);
        }
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter) {
            fprintf(tty_out, SHOW"\r\n");
            return (Tristate)(pos + required);
        }

        if (k.type == key_arrow_up) {
            if (pos == 0) pos = OPTS_LEN - required;
            else pos--;
        }
        else if (k.type == key_arrow_down) {
            if (pos == OPTS_LEN - required) pos = 0;
            else pos++;
        }
        fprintf(tty_out, UP(%d), OPTS_LEN + !required);
    }
}

void read_select(const Field *f, char *buffer) {
    NOB_ASSERT(f->type == ft_select);

    SelectOptions opts = f->select.options;
    bool required = f->select.required;
    fprintf(tty_out, HIDE"%s%s", f->select.question, required ? "*" : "");

    size_t pos = 0;
    while (1) {
        if (!required) {
            fprintf(tty_out, "\r\n%s", pos == 0 ? PROMPT : " ");
        }
        for (size_t i = !required; i < opts.count + !required; i++) {
            fprintf(tty_out, "\r\n%s %s", pos == i ? PROMPT : " ", opts.items[i - !required]);
        }
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter) {
            if (pos == 0) {
                buffer[0] = 0;
            }
            else {
                const char* selection = opts.items[pos - !required];
                size_t len = strlen(selection)+1;
                strlcpy(buffer, selection, len);
            }
            fprintf(tty_out, SHOW"\r\n");
            return;
        }
        if (k.type == key_tab) {
            fprintf(tty_out, SHOW);
            buffer[0] = 0;
            return;
        }

        if (k.type == key_arrow_up) {
            if (pos == 0) pos = opts.count - required;
            else pos--;
        }
        else if (k.type == key_arrow_down) {
            if (pos == opts.count - required) pos = 0;
            else pos++;
        }
        fprintf(tty_out, UP(%zu), opts.count + !required);
    }
}

void read_multiselect(const Field *f, SelectOptions *selected_opts) {
    NOB_ASSERT(f->type == ft_multiselect);

    MultiSelectFieldMembers p = f->multiselect;
    fprintf(tty_out, HIDE"%s ", p.question);
    if (p.max == INT_MAX) {
        if (p.min == 0) fprintf(tty_out, "(any)");
        else fprintf(tty_out, "(at least %d)", p.min);
    }
    else {
        fprintf(tty_out, "(%d-%d)", p.min, p.max);
    }

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
            fprintf(tty_out, "\r\n%s %s %s",
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
                fprintf(tty_out, UP(%zu), opts.count);
                continue;
            }
            fprintf(tty_out, SHOW"\r\n");
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
            if (pos == 0) pos = opts.count - 1;
            else pos--;
        }
        else if (k.type == key_arrow_down) {
            if (pos == opts.count - 1) pos = 0;
            else pos++;
        }
        fprintf(tty_out, UP(%zu), opts.count);
    }
}

int64_t read_counter(const Field *f) {
    NOB_ASSERT(f->type == ft_counter);

    fprintf(tty_out, HIDE"%s\r\n0", f->counter.question);
    fflush(tty_out);

    int64_t value = 0;
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
            if (value < INT64_MAX) value++;
        }
        else if (k.type == key_arrow_down) {
            if (value > 0) value--;
        }
        else {
            continue;
        }

        fprintf(tty_out, "\r%lld"CLRDOWN, value);
        fflush(tty_out);
    }
}

static bool fails_text_checks(const Field *f, char *answer) {
    NOB_ASSERT(f->type == ft_text);
    TextFieldMembers p = f->text;
    return is_empty(answer)
        ? p.required
        : !is_match(&p.regex, answer);
}

static void place_char_in_text_buffer(TextBuffer *tb, char c) {
    if (tb->position < tb->end) {
        for (size_t i = tb->end + 1; i >= tb->position + 1; i--) {
            tb->buffer[i] = tb->buffer[i - 1];
        }
    }
    tb->buffer[tb->position++] = c;
    tb->buffer[++tb->end] = '\0';
}

static void reset_text_buffer(TextBuffer *tb) {
    tb->buffer[0] = '\0';
    tb->position = 0;
    tb->end = 0;
}

static void handle_text_buffer(TextBuffer *tb, KeyType kt) {
    if (kt == key_escape) {
        reset_text_buffer(tb);
    }
    else if (kt == key_backspace) {
        if (tb->position > 0) {
            for (size_t i = tb->position; i < tb->end; i++) {
                tb->buffer[i - 1] = tb->buffer[i];
            }
            tb->position--;
            tb->buffer[--tb->end] = '\0';
        }
    }
    else if (kt == key_ctrl_backspace) {
        int orig_pos = (int) tb->position;
        while (tb->position > 0 && is_word_boundary(tb->buffer[tb->position - 1])) tb->position--;
        while (tb->position > 0 && !is_word_boundary(tb->buffer[tb->position - 1])) tb->position--;
        int del_len = orig_pos - (int) tb->position;
        for (size_t i = (size_t) orig_pos; i < tb->end; i++) {
            tb->buffer[i - del_len] = tb->buffer[i];
        }
        tb->end -= (size_t) del_len;
        tb->buffer[tb->end] = '\0';
    }
    else if (kt == key_delete) {
        if (tb->position < tb->end) {
            for (size_t i = tb->position + 1; i < tb->end; i++) {
                tb->buffer[i - 1] = tb->buffer[i];
            }
            tb->buffer[--tb->end] = '\0';
        }
    }
    else if (kt == key_ctrl_delete) {
        int orig_pos = (int) tb->position;
        while (tb->position < tb->end && !is_word_boundary(tb->buffer[tb->position])) tb->position++;
        while (tb->position < tb->end && is_word_boundary(tb->buffer[tb->position])) tb->position++;
        int del_len = (int) tb->position - orig_pos;
        for (size_t i = tb->position; i < tb->end; i++) {
            tb->buffer[i - del_len] = tb->buffer[i];
        }
        tb->end -= (size_t) del_len;
        tb->position = (size_t) orig_pos;
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
        while (tb->position > 0 && is_word_boundary(tb->buffer[tb->position - 1])) tb->position--;
        while (tb->position > 0 && !is_word_boundary(tb->buffer[tb->position - 1])) tb->position--;
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
        if (tb.end == 0 && ph) {
            fprintf(tty_out, FAINT"%s"RESET, ph);
        }
        else {
            fprintf(tty_out, "%s", buffer);
        }
        fprintf(tty_out, CLRDOWN"\r"RIGHT(%zu), tb.position + 3);
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter) {
            if (failed_checks) continue;
            tb.buffer[tb.end] = '\0';
            write_nl(tty_out);
            return;
        }

        if (k.type == key_char && tb.position < f->text.maxlength - 1) {
            place_char_in_text_buffer(&tb, (char) k.ch);
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
        fprintf(tty_out, "\r%s %s"CLRDOWN"\r"RIGHT(%zu), failed_checks ? ERR_PROMPT : PROMPT, buffer, tb.position + 3);
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter) {
            if (failed_checks) continue;
            tb.buffer[tb.end] = '\0';
            write_nl(tty_out);
            return;
        }

        if (k.type == key_char && (k.ch == '-' || k.ch == '.' || isdigit(k.ch))) {
            place_char_in_text_buffer(&tb, (char) k.ch);
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
        snprintf(buffer, ANSWER_BUFFER_LEN, "%g", CLAMP(current + signed_step, p.min, p.max));
        tb.position = strlen(buffer);
        tb.end = tb.position;
    }
}

static bool fails_multitext_checks(const Field *f, const char *answer) {
    NOB_ASSERT(f->type == ft_multitext);

    MultiTextFieldMembers p = f->multitext;

    Nob_String_View answer_sv = nob_sv_from_cstr(answer);
    int count = 0;
    while (answer_sv.count > 0) {
        Nob_String_View sv = nob_sv_chop_by_delim(&answer_sv, ',');
        if (sv.count > p.maxlength || (++count) > p.max || !is_sv_match(&p.regex, sv)) return true;
    }
    return count < p.min;
}

void read_multitext(const Field *f, char* buffer) {
    NOB_ASSERT(f->type == ft_multitext);

    MultiTextFieldMembers p = f->multitext;

    TextBuffer tb = {
        .position = 0,
        .end = 0,
        .buffer = buffer,
    };

    fprintf(tty_out, "%s%s\r\n", p.question, p.required ? "*" : "");
    if (p.max == INT_MAX) {
        if (p.min == 0) fprintf(tty_out, "(any)\r\n");
        else fprintf(tty_out, "(at least %d)\r\n", p.min);
    }
    else {
        fprintf(tty_out, "(%d-%d)\r\n", p.min, p.max);
    }

    const char *ph = p.placeholder;
    while (1) {
        bool failed_checks = fails_multitext_checks(f, buffer);
        fprintf(tty_out, "\r%s ", failed_checks ? ERR_PROMPT : PROMPT);
        if (tb.end == 0 && ph) {
            fprintf(tty_out, FAINT"%s"RESET, ph);
        }
        else {
            fprintf(tty_out, "%s", buffer);
        }
        fprintf(tty_out, CLRDOWN"\r"RIGHT(%zu), tb.position + 3);
        fflush(tty_out);

        Key k = read_key(tty_in);
        if (k.type == key_exit) user_exit();
        if (k.type == key_enter) {
            if (failed_checks) continue;
            tb.buffer[tb.end] = '\0';
            write_nl(tty_out);
            return;
        }

        if (k.type == key_char) {
            place_char_in_text_buffer(&tb, (char) k.ch);
            continue;
        }

        handle_text_buffer(&tb, k.type);
    }
}
