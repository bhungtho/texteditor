#include "te.h"

/* editor operations */

void editor_insert_char(int c) {
    // if at right end of file, need to append another row
    if(E.cy == E.num_rows) {
        editor_append_row("", 0);
    }
    editor_row_insert_char(&E.row[E.cy], E.cx, c);
    E.cx++;
}

/* append buffer */

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

/* input */

void editor_move_cursor(int key) {
    editor_row * row = (E.cy >= E.num_rows) ? NULL : &E.row[E.cy];

    switch(key) {
        case ARROW_LEFT:
            if(E.cx != 0) {
                E.cx--;
            }
            else if(E.cy > 0) {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if(row && E.cx < row->size) {
                E.cx++;
            }
            else if(row && E.cx == row->size) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if(E.cy != 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if(E.cy < E.num_rows) {
                E.cy++;
            }
            break;
    }

    row = (E.cy >= E.num_rows) ? NULL : &E.row[E.cy];
    int row_length = row ? row->size: 0;
    if(E.cx > row_length) {
        E.cx = row_length;
    }
}

void editor_process_keypress() {
    int c = editor_read_key();

    switch(c) {
        case '\r':
            break;
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if(E.cy < E.num_rows) {
                E.cx = E.row[E.cy].size;
            }
            break;
        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            break;
        case PAGE_UP:
        case PAGE_DOWN:
        {
            if(c == PAGE_UP) {
                E.cy = E.row_offset;
            }
            else if(c == PAGE_DOWN) {
                E.cy = E.row_offset + E.screen_rows - 1;
                if(E.cy > E.num_rows) {
                    E.cy = E.num_rows;
                }
            }
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
        case CTRL_KEY('l'):
        case '\x1b':
            break;
        default:
            editor_insert_char(c);
            break;
    }
}

/* output */

void editor_draw_message_bar(append_buffer * ab) {
    ab_append(ab, "\x1b[K", 3);
    int msg_length = strlen(E.status_msg);
    if(msg_length > E.screen_cols) {
        msg_length = E.screen_cols;
    }
    if(msg_length && time(NULL) - E.status_time < 5) {
        ab_append(ab, E.status_msg, msg_length);
    }
}

void editor_set_status(const char * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.status_msg, sizeof(E.status_msg), fmt, ap);
    va_end(ap);
    E.status_time = time(NULL);
}

void editor_draw_status_bar(append_buffer * ab) {
    // invert colors
    ab_append(ab, "\x1b[7m", 4);
    
    char status[80];
    char r_status[80];
    int length = snprintf(status, sizeof(status), "%.20s - %d lines", E.file_name ? E.file_name : "[No Name]", E.num_rows);
    int r_length = snprintf(r_status, sizeof(r_status), "%d/%d", E.cy + 1, E.num_rows);
    
    if(length > E.screen_cols) {
        length = E.screen_cols;
    }
    ab_append(ab, status, length);
    while(length < E.screen_cols) {
        if(E.screen_cols - length == r_length) {
            ab_append(ab, r_status, r_length);
            break;
        }
        else {
            ab_append(ab, " ", 1);
            length++;
        }
    }
    // revert back to original colors
    ab_append(ab, "\x1b[m", 3);
    ab_append(ab, "\r\n", 2);
}

void editor_scroll() {
    E.rx = 0;
    if(E.cy < E.num_rows) {
        E.rx = cx_to_rx(&E.row[E.cy], E.cx);
    }
    // if cursor above visible window
    if(E.cy < E.row_offset) {
        E.row_offset = E.cy;
    }
    // if cursor below visible window
    if(E.cy >= E.row_offset + E.screen_rows) {
        E.row_offset = E.cy - E.screen_rows + 1;
    }

    // if cursor left of visible window
    if(E.rx < E.col_offset) {
        E.col_offset = E.rx;
    }
    // if cursor right of visible window
    if(E.rx >= E.col_offset + E.screen_cols) {
        E.col_offset = E.rx - E.screen_cols + 1;
    }
}

void editor_refresh_screen() {
    editor_scroll();
    append_buffer ab = ABUF_INIT;

    // hide cursor
    ab_append(&ab, "\x1b[?25l", 6);
    //ab_append(&ab, "\x1b[2J", 4);
    ab_append(&ab, "\x1b[H", 3);

    editor_draw_rows(&ab);
    editor_draw_status_bar(&ab);
    editor_draw_message_bar(&ab);

    // move cursor to (cx, cy)
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy - E.row_offset + 1, E.rx - E.col_offset + 1);
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
        int file_row = y + E.row_offset;
        if(file_row >= E.num_rows) {
            // check if we're currently drawing a row that is part of the text buffer
            if(y >= E.num_rows) {
                if(E.num_rows == 0 && y == E.screen_rows / 3) {
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
            }
        }
        else {
            int length = E.row[file_row].r_size - E.col_offset;
            if(length < 0) {
                length = 0;
            }
            if(length > E.screen_cols) {
                length = E.screen_cols;
            }
            ab_append(ab, &E.row[file_row].render[E.col_offset], length);
        }

        ab_append(ab, "\x1b[K", 3);
        //if(y < E.screen_rows - 1) {
            ab_append(ab, "\r\n", 2);
        //}
    }
}

/* terminal */

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

/* row operations */

void editor_row_insert_char(editor_row * row, int at, int c) {
    // check if at is valid
    if(at < 0 || at > row->size) {
        at = row->size;
    }

    // make room for one more byte for chars + null byte
    row->chars = realloc(row->chars, row->size + 2);
    // make room for new char
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    // insert char
    row->chars[at] = c;
    // update row
    editor_update_row(row);
}

int cx_to_rx(editor_row * row, int cx) {
    int rx = 0;
    int j;
    for(j = 0; j < cx; j++) {
        if(row->chars[j] == '\t') {
            rx += (TAB_STOP - 1) - (rx % TAB_STOP);
        }
        rx++;
    }
    return rx;
}

void editor_update_row(editor_row * row) {
    int tabs = 0;
    int j;
    for(j = 0; j < row->size; j++) {
        if(row->chars[j] == '\t') {
            tabs++;
        }
    }
    free(row->render);
    row->render = malloc(row->size + tabs * (TAB_STOP - 1) + 1);

    int index = 0;
    for(j = 0; j < row->size; j++) {
        if(row->chars[j] == '\t') {
            row->render[index++] = ' ';
            while(index % TAB_STOP != 0) {
                row->render[index++] = ' ';
            }
        }
        else {
            row->render[index++] = row->chars[j];
        }
    }
    row->render[index] = '\0';
    row->r_size = index;
}

void editor_append_row(char * s, size_t length) {
    E.row = realloc(E.row, sizeof(editor_row) * (E.num_rows + 1));

    int at = E.num_rows;
    E.row[at].size = length;
    E.row[at].chars = malloc(length + 1);
    memcpy(E.row[at].chars, s, length);
    E.row[at].chars[length] = '\0';

    E.row[at].r_size = 0;
    E.row[at].render = NULL;
    editor_update_row(&E.row[at]);

    E.num_rows++;
}

/* file i/o */

void editor_open(char * file_name) {
    free(E.file_name);
    E.file_name = strdup(file_name);

    FILE * fp = fopen(file_name, "r");
    if(!fp) {
        die("fopen");
    }

    char * line = NULL;
    size_t line_cap = 0;
    ssize_t line_length;
    line_length = getline(&line, &line_cap, fp);
    while((line_length = getline(&line, &line_cap, fp)) != -1) {
        while(line_length > 0 && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r')) {
            line_length--;
        }
        editor_append_row(line, line_length);
    }
    free(line);
    fclose(fp);
}

/* initialization */

void init_editor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.row_offset = 0;
    E.col_offset = 0;
    E.num_rows = 0;
    E.row = NULL;
    E.file_name = NULL;
    E.status_msg[0] = '\0';
    E.status_time = 0;

    if(get_window_size(&E.screen_rows, &E.screen_cols) == -1) {
        die("get_window_size");
    }
    E.screen_rows -= 2;
}

int main(int argc, char * argv[]) {
    enable_raw_mode();
    init_editor();
    if(argc >= 2) {
        editor_open(argv[1]);
    }

    editor_set_status("HELP: Ctrl-Q = quit");

    // reads user input until a q is seen
    while(1) {
        editor_refresh_screen();
        editor_process_keypress();
    }

    return 0;
}