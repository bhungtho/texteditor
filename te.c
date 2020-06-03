#include "te.h"

// append buffer

void ab_append(append_buffer * ab, const char * s, int len) {
    char * new = realloc(ab->b, ab->len + len);

    if(new == NULL) {
        return;
    }
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void ab_free(append_buffer * ab) {
    free(ab->b);
}

// input

void editor_move_cursor(int key) {
    switch(key) {
        case ARROW_LEFT:
            if(E.cx != 0) {
                E.cx--;
            }
            break;
        case ARROW_RIGHT:
            if(E.cx != E.screen_cols - 1) {
                E.cx++;
            }
            break;
        case ARROW_UP:
            if(E.cy != 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if(E.cy != E.screen_rows - 1) {
                E.cy++;
            }
            break;
    }
}

void editor_process_keypress() {
    int c = editor_read_key();

    switch(c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            E.cx = E.screen_cols - 1;
            break;
        case PAGE_UP:
        case PAGE_DOWN:
        {
            int times = E.screen_rows;
            while(times--) {
                editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        }
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editor_move_cursor(c);
            break;
    }
}

// output

void editor_refresh_screen() {
    append_buffer ab = ABUF_INIT;

    // hide cursor
    ab_append(&ab, "\x1b[?25l", 6);
    //ab_append(&ab, "\x1b[2J", 4);
    ab_append(&ab, "\x1b[H", 3);

    editor_draw_rows(&ab);

    // move cursor to (cx, cy)
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    ab_append(&ab, buf, strlen(buf));

    //ab_append(&ab, "\x1b[H", 3);
    
    // show cursor
    ab_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

void editor_draw_rows(append_buffer * ab) {
    int y;
    for(y = 0; y < E.screen_rows; y++) {
        if(y == E.screen_rows / 3) {
            char welcome[80];
            int welcome_len = snprintf(welcome, sizeof(welcome), "Ben's TE -- version %s", VERSION);
            // truncate if too long
            if(welcome_len > E.screen_cols) {
                welcome_len = E.screen_cols;
            }
            int padding = (E.screen_cols - welcome_len) / 2;
            if(padding) {
                ab_append(ab, "~", 1);
                padding--;
            } 
            while(padding--) {
                ab_append(ab, " ", 1);
            }
            ab_append(ab, welcome, welcome_len);
        }
        else {
            ab_append(ab, "~", 1);
        }

        ab_append(ab, "\x1b[K", 3);
        if(y < E.screen_rows - 1) {
            ab_append(ab, "\r\n", 2);
        }
    }
}

// terminal

int get_window_size(int * rows, int * cols) {
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }
        editor_read_key();
        return -1;
    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

int editor_read_key() {
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) {
            die("read");
        }
    }
    // if arrow key
    if(c == '\x1b') {
        char seq[3];

        if(read(STDIN_FILENO, &seq[0], 1) != 1) {
            return '\x1b';
        }
        if(read(STDIN_FILENO, &seq[1], 1) != 1) {
            return '\x1b';
        }

        if(seq[0] == '[') {
            if(seq[1] >= '0' && seq[1] <= '9') {
                if(read(STDIN_FILENO, &seq[2], 1) != 1) {
                    return '\x1b';
                }
                if(seq[2] == "~") {
                    switch(seq[1]) {
                        case '1':
                            return HOME_KEY;
                        case '3':
                            return DEL_KEY;
                        case '4':
                            return END_KEY;
                        case '5':
                            return PAGE_UP;
                        case '6':
                            return PAGE_DOWN;
                        case '7':
                            return HOME_KEY;
                        case '8':
                            return END_KEY;
                    }
                }
            }
            else {
                switch(seq[1]) {
                    case 'A':
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_RIGHT;
                    case 'D':
                        return ARROW_LEFT;
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                }
            }
        }
        else if(seq[0] == 'O') {
            switch(seq[1]) {
                case 'H': 
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
            }
        }
        return '\x1b';
    }
    return c;
}

void enable_raw_mode() {
    // reads terminal attributes into raw
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr");
    }
    
    // automatically call disable_raw_mode when the program exits
    atexit(disable_raw_mode);

    struct termios raw = E.orig_termios;

    // local flags
    // ECHO - turn off echoing
    // ICANON - turn off canonical mode
    // ISIG - turn off ctrl-c & ctrl-z
    // IEXTEN - disable ctrl-v
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    // input flag
    // IXON - disable ctrl-s & ctrl-q
    // ICRNL - fix ctrl-m
    // BRKINT - stop break conditions
    // INPCK - disable parity checking
    // ISTRIP - disables stripping of 8th bit
    raw.c_iflag &= ~(IXON | ISTRIP | INPCK | ICRNL | BRKINT);

    // output flag
    // OPOST - turn off output processing (\r\n)
    raw.c_oflag &= ~(OPOST);

    // CS8 - sets character size to 8 bits per byte
    raw.c_cflag |= (CS8);

    // time-in/time-out values
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    // sets terminal attributes from raw
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

void disable_raw_mode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

// initialization

void init_editor() {
    E.cx = 0;
    E.cy = 0;
    if(get_window_size(&E.screen_rows, &E.screen_cols) == -1) {
        die("get_window_size");
    }
}

int main() {
    enable_raw_mode();
    init_editor();

    // reads user input until a q is seen
    while(1) {
        editor_refresh_screen();
        editor_process_keypress();
    }

    return 0;
}