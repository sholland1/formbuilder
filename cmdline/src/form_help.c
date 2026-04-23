#include "form_cli.h"

// Use when there are no arrows used
static void print_key(FILE *stream, const char *keymap, const char *action) {
    fprintf(stream, "  %-18s %s\r\n", keymap, action);
}

// Use when there is one arrow used
static void print_key1(FILE *stream, const char *keymap, const char *action) {
    fprintf(stream, "  %-20s %s\r\n", keymap, action);
}

// Use when there are two arrows used
static void print_key2(FILE *stream, const char *keymap, const char *action) {
    fprintf(stream, "  %-22s %s\r\n", keymap, action);
}

static void print_no_keys(FILE *stream, const char *reason) {
    print_key(stream, "(no keymaps)", reason);
}

static void print_help_text(FILE *stream) {
    print_key(stream, "Type text", "Insert character at cursor");
    print_key(stream, "Enter", "Submit when input is valid");
    print_key(stream, "Esc", "Clear entire input");
    print_key(stream, "Backspace", "Delete character before cursor");
    print_key(stream, "Ctrl+Backspace", "Delete previous word");
    print_key(stream, "Delete", "Delete character at cursor");
    print_key(stream, "Ctrl+Delete", "Delete next word");
    print_key2(stream, "← / →", "Move cursor by character");
    print_key2(stream, "Ctrl+← / Ctrl+→", "Move cursor by word");
    print_key(stream, "Home / End", "Jump to start/end of input");
    print_key(stream, "Ctrl+C / Ctrl+D", "Exit form immediately");
}

static void print_help_number(FILE *stream) {
    print_key(stream, "Digits, -, .", "Insert numeric characters");
    print_key(stream, "Enter", "Submit when input is valid");
    print_key2(stream, "↑ / ↓", "Increase/decrease by step (clamped)");
    print_key(stream, "Esc", "Clear entire input");
    print_key(stream, "Backspace", "Delete character before cursor");
    print_key(stream, "Ctrl+Backspace", "Delete previous word");
    print_key(stream, "Delete", "Delete character at cursor");
    print_key(stream, "Ctrl+Delete", "Delete next word");
    print_key2(stream, "← / →", "Move cursor by character");
    print_key2(stream, "Ctrl+← / Ctrl+→", "Move cursor by word");
    print_key(stream, "Home / End", "Jump to start/end of input");
    print_key(stream, "Ctrl+C / Ctrl+D", "Exit form immediately");
}

static void print_help_select(FILE *stream) {
    print_key2(stream, "↑ / ↓", "Move selection");
    print_key(stream, "Enter", "Select current option");
    print_key(stream, "Tab", "Skip field (sets null/empty)");
    print_key(stream, "Ctrl+C / Ctrl+D", "Exit form immediately");
}

static void print_help_multiselect(FILE *stream) {
    print_key2(stream, "↑ / ↓", "Move selection cursor");
    print_key(stream, "Space", "Toggle current option");
    print_key(stream, "Enter", "Submit selected options if valid");
    print_key(stream, "Tab", "Submit selected options if valid");
    print_key(stream, "Ctrl+C / Ctrl+D", "Exit form immediately");
}

static void print_help_multitext(FILE *stream) {
    print_key(stream, "Type text", "Insert character at cursor");
    print_key(stream, "Enter", "Submit when input is valid");
    print_key(stream, "Esc", "Clear entire input");
    print_key(stream, "Backspace", "Delete character before cursor");
    print_key(stream, "Ctrl+Backspace", "Delete previous word");
    print_key(stream, "Delete", "Delete character at cursor");
    print_key(stream, "Ctrl+Delete", "Delete next word");
    print_key2(stream, "← / →", "Move cursor by character");
    print_key2(stream, "Ctrl+← / Ctrl+→", "Move cursor by word");
    print_key(stream, "Home / End", "Jump to start/end of input");
    print_key(stream, "Ctrl+C / Ctrl+D", "Exit form immediately");
}

static void print_help_timestamp(FILE *stream) {
    print_no_keys(stream, "Timestamp value is auto-generated.");
}

static void print_help_date(FILE *stream) {
    print_key2(stream, "← / →", "Switch between month/day/year");
    print_key2(stream, "↑ / ↓", "Adjust current month/day/year value");
    print_key(stream, "Enter", "Submit date (or null when optional)");
    print_key(stream, "Esc", "Reset to defaults");
    print_key(stream, "Ctrl+C / Ctrl+D", "Exit form immediately");
}

static void print_help_counter(FILE *stream) {
    print_key1(stream, "↑ or Space", "Increase counter by 1");
    print_key1(stream, "↓", "Decrease counter by 1");
    print_key(stream, "Esc", "Reset counter to 0");
    print_key(stream, "Enter", "Submit counter value");
    print_key(stream, "Ctrl+C / Ctrl+D", "Exit form immediately");
}

static void print_help_color(FILE *stream) {
    print_key(stream, "Hex digit (0-9, a-f)", "Set active nibble");
    print_key2(stream, "← / →", "Move to previous/next hex nibble");
    print_key2(stream, "↑ / ↓", "Increase/decrease active nibble");
    print_key(stream, "Tab", "Jump to next color component");
    print_key(stream, "Shift+Tab", "Jump to previous color component");
    print_key(stream, "Enter", "Submit color value");
    print_key(stream, "Ctrl+C / Ctrl+D", "Exit form immediately");
}

static void print_help_bool(FILE *stream) {
    print_key2(stream, "↑ / ↓", "Move selection");
    print_key(stream, "Enter", "Submit selected value");
    print_key(stream, "Ctrl+C / Ctrl+D", "Exit form immediately");
}

static void print_help_timer(FILE *stream) {
    print_key(stream, "Space", "Start/stop timer");
    print_key(stream, "Esc", "Reset timer to 0");
    print_key(stream, "Enter", "Submit elapsed time");
    print_key(stream, "Ctrl+C / Ctrl+D", "Exit form immediately");
}

static void print_help_guid(FILE *stream) {
    print_no_keys(stream, "GUID value is auto-generated.");
}

static void print_help_rating(FILE *stream) {
    print_key2(stream, "↑ / →", "Increase rating by one step");
    print_key2(stream, "↓ / ←", "Decrease rating by one step");
    print_key2(stream, "Shift+↑ / Shift+→", "Increase by one whole point");
    print_key2(stream, "Shift+↓ / Shift+←", "Decrease by one whole point");
    print_key(stream, "Esc", "Reset rating to 0");
    print_key(stream, "Enter", "Submit rating");
    print_key(stream, "Ctrl+C / Ctrl+D", "Exit form immediately");
}

#define X(name) static void print_help_##name(FILE *stream) { \
    print_no_keys(stream, #name" field is unimplemented in the CLI and is skipped."); }
        UNIMPLEMENTED_FIELDTYPES
#undef X

void display_help(FieldType type) {
    fprintf(tty_out, MODAL);
    fprintf(tty_out, "Help\n\r");

    switch (type) {
#define X(name) case ft_##name: print_help_##name(tty_out); break;
        FIELDTYPES
#undef X
        default: NOB_UNREACHABLE("Unidentified type!");
    }

    fflush(tty_out);
    read_key(tty_in);
    fprintf(tty_out, UNMODAL);
    fflush(tty_out);
}
