#include "form_cli.h"

#include <stdlib.h>
#include <unistd.h>

struct termios original_termios;
FILE *tty_in = NULL;
FILE *tty_out = NULL;

void write_nl(FILE *stream) {
    putc('\r', stream);
    putc('\n', stream);
    fflush(stream);
}

void user_exit(void) {
    write_nl(tty_out);
    exit(EXIT_FAILURE);
}

Key read_key(FILE *stream) {
    uint8_t buffer[6];
    int count = read(fileno(stream), &buffer, 6);
    if (count == 0) return KEY_EOF;

    uint8_t c = buffer[0];
    if (count == 1) {
        if (c == 3 || c == 4) return KEY(key_exit);
        if (c == '\n' || c == '\r') return KEY(key_enter);
        if (c == '\t') return KEY(key_tab);
        if (c >= 32 && c <= 126) return (Key){.type = key_char, .ch = c};
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

    if (count == 6 && c == '1' && buffer[3] == ';' && buffer[4] == '2') {
        c = buffer[5];
        if (c == 'A') return KEY(key_shift_up);
        if (c == 'B') return KEY(key_shift_down);
        if (c == 'C') return KEY(key_shift_right);
        if (c == 'D') return KEY(key_shift_left);
    }
    else if (count == 6 && c == '1' && buffer[3] == ';' && buffer[4] == '5') {
        c = buffer[5];
        if (c == 'C') return KEY(key_ctrl_right);
        if (c == 'D') return KEY(key_ctrl_left);
    }
    return KEY(key_unknown);
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
    tty_in = fopen("/dev/tty", "r");
    tty_out = fopen("/dev/tty", "w");
    if (!tty_in || !tty_out) {
        perror("Failed to open /dev/tty");
        exit(EXIT_FAILURE);
    }

    if (tcgetattr(fileno(tty_in), &original_termios) == -1) {
        perror("tcgetattr");
        terminal_deinit();
        exit(EXIT_FAILURE);
    }

    struct termios raw = original_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(fileno(tty_in), TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        terminal_deinit();
        exit(EXIT_FAILURE);
    }

    if (atexit(terminal_deinit) != 0) {
        fprintf(stderr, "atexit failed\n");
        terminal_deinit();
        exit(EXIT_FAILURE);
    }
}
