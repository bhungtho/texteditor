#include "te.h"

/* syntax highlighting */

void editor_select_syntax_highlight() {
    E.syntax = NULL;
    if(E.file_name == NULL) {
        return;
    }

    char * ext = strrchr(E.file_name, '.');

    for(unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        editor_syntax * s = &HLDB[j];
        unsigned int i = 0;
        while(s->file_match[i]) {
            int is_ext = (s->file_match[i][0] == '.');
            if((is_ext && strstr(E.file_name, s->file_match[i]))) {
                E.syntax = s;
                int file_row;
                for(file_row = 0; file_row < E.num_rows; file_row++) {
                    editor_update_syntax(&E.row[file_row]);
                }
                return;
            }
            i++;
        }
    }
}

int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

int editor_syntax_to_color(int hl) {
    switch(hl) {
        case HL_COMMENT:
        case HL_MLCOMMENT:
            // cyan
            return 36;
        case HL_KEYWORD1:
            // yellow
            return 33;
        case HL_KEYWORD2:
            // green
            return 32;
        case HL_STRING:
            // magenta
            return 35;
        case HL_NUMBER: 
            // red
            return 31;
        case HL_MATCH:
            // blue
            return 34;
        default: 
            return 37;
    }
}

void editor_update_syntax(editor_row * row) {
    row->hl = realloc(row->hl, row->r_size);
    memset(row->hl, HL_NORMAL, row->r_size);

    if(E.syntax == NULL) {
        return;
    }

    char ** keywords = E.syntax->keywords;

    // alias set-up
    char * scs = E.syntax->singleline_comment_start;
    char * mcs = E.syntax->multiline_comment_start;
    char * mce = E.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->index > 0 && E.row[row->index - 1].hl_open_comment);

    int i = 0;
    while (i < row->r_size) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if(scs_len && !in_string && !in_comment) {
            if(!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->hl[i], HL_COMMENT, row->r_size - i);
                break;
            }
        }

        if(mcs_len && mce_len && !in_string) {
            if(in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if(!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                }
                else {
                    i++;
                    continue;
                }
            }
            else if(!strncmp(&row->render[i], mcs, mcs_len)) {
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if(E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if(in_string) {
                row->hl[i] = HL_STRING;
                if(c == '\\' && i + 1 < row->r_size) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if(c == in_string) {
                    row->hl[i] = HL_NORMAL;
                    in_string = 0;
                }
                i++;
                prev_sep = 1;
                continue;
            }
            else {
                if(c == '"' || c == '\'') {
                    in_string = c;
                    row->hl[i] == HL_STRING;
                    i++;
                    continue;
                }
            }
        }

        if(E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || (c == '.' && prev_hl == HL_NUMBER)) {
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if(prev_sep) {
            int j = 0;
            for(j = 0; keywords[j]; j++) {
                int k_length = strlen(keywords[j]);
                int kw2 = keywords[j][k_length - 1] == '|';
                if(kw2) {
                    k_length--;
                }

                if(!strncmp(&row->render[i], keywords[j], k_length) && is_separator(row->render[i + k_length])) {
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, k_length);
                    i += k_length;
                    break;
                }
            }
            if(keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }
        prev_sep = is_separator(c);
        i++;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if(changed && row->index + 1 < E.num_rows) {
        editor_update_syntax(&E.row[row->index + 1]);
    }
}

/* find */

void editor_find_callback(char * query, int key) {
    static int last_match = -1;     // index of the row the last match was on
    static int direction = 1;      // 1 if searching forward, -1 if searching backwards

    static int saved_hl_line;
    static char * saved_hl = NULL;

    if(saved_hl) {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].r_size);
        free(saved_hl);
        saved_hl = NULL;
    }

    if(key == '\r' || key == '\x1b') {
        last_match = -1;
        direction = 1;
        return;
    }
    else if(key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    }
    else if(key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    }
    else {
        last_match = -1;
        direction = 1;
    }

    if(last_match == -1) {
        direction = 1;
    }
    int current = last_match;   // index of the current row we're searching
    int i;
    for(i = 0; i < E.num_rows; i++) {
        current += direction;
        if(current == -1) {
            current = E.num_rows - 1;
        }
        else if(current == E.num_rows) {
            current = 0;
        }        
        editor_row * row = &E.row[current];
        // check if query is a substring of the current row
        char * match = strstr(row->render, query);
        if(match) {
            last_match = current;
            E.cy = current;
            E.cx = rx_to_cx(row, match - row->render);
            E.row_offset = E.num_rows;

            // save previous highlighting
            saved_hl_line = current;
            saved_hl = malloc(row->r_size);
            memcpy(saved_hl, row->hl, row->r_size);
            // syntax highlighting
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

void editor_find() {
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_col_offset = E.col_offset;
    int saved_row_offset = E.row_offset;

    char * query = editor_prompt("Search: %s (Use ESC/Arrows/Enter)", editor_find_callback);
    if(query) {
        free(query);
    }
    else {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.col_offset = saved_col_offset;
        E.row_offset = saved_row_offset;
    }
}

/* editor operations */

void editor_insert_new_line() {
    if(E.cx == 0) {
        editor_insert_row(E.cy, "", 0);
    }
    else {
        editor_row * row = &E.row[E.cy];
        editor_insert_row(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editor_update_row(row);
    }
    E.cy++;
    E.cx = 0;
}

void editor_delete_char() {
    if(E.cy == E.num_rows) {
        return;
    }
    if(E.cx == 0 && E.cy == 0) {
        return;
    }

    editor_row * row = &E.row[E.cy];
    if(E.cx > 0) {
        editor_row_delete_char(row, E.cx - 1);
        E.cx--;
    }
    else {
        E.cx = E.row[E.cy - 1].size;
        editor_row_append_string(&E.row[E.cy - 1], row->chars, row->size);
        editor_delete_row(E.cy);
        E.cy--;
    }
}

void editor_insert_char(int c) {
    // if at right end of file, need to append another row
    if(E.cy == E.num_rows) {
        editor_insert_row(E.num_rows, "", 0);
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

char * editor_prompt(char * prompt, void (* callback)(char *, int)) {
    size_t buffer_size = 128;
    char * buffer = malloc(buffer_size);

    size_t buffer_length = 0;
    buffer[0] = '\0';

    while(1) {
        editor_set_status(prompt, buffer);
        editor_refresh_screen();

        int c = editor_read_key();
        if(c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if(buffer_length != 0) {
                buffer[--buffer_length] = '\0';
            }
        }
        else if(c == '\x1b') {
            editor_set_status("");
            if(callback) {
                callback(buffer, c);
            }
            free(buffer);
            return NULL;
        }
        else if(c == '\r') {
            if(buffer_length != 0) {
                editor_set_status("");
                if(callback) {
                    callback(buffer, c);
                }
                return buffer;
            }
        }
        else if(!iscntrl(c) && c < 128) {
            if(buffer_length == buffer_size - 1) {
                buffer_size *= 2;
                buffer = realloc(buffer, buffer_size);
            }
            buffer[buffer_length++] = c;
            buffer[buffer_length] = '\0';
        }
        if(callback) {
            callback(buffer, c);
        }
    }
}

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
    static int quit_times = QUIT_TIMES;

    int c = editor_read_key();

    switch(c) {
        case '\r':
        editor_insert_new_line();
            break;
        case CTRL_KEY('q'):
            if(E.dirty && quit_times > 0) {
                editor_set_status("WARNING! File has unsaved changes. Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case CTRL_KEY('s'):
            //printf("reached");
            editor_save();
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if(E.cy < E.num_rows) {
                E.cx = E.row[E.cy].size;
            }
            break;
        case CTRL_KEY('f'):
            editor_find();
            break;
        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if(c == DEL_KEY) {
                editor_move_cursor(ARROW_RIGHT);
            }
            editor_delete_char();
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
    quit_times = QUIT_TIMES;
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
    int length = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.file_name ? E.file_name : "[No Name]", E.num_rows, E.dirty ? "(modified)" : "");
    int r_length = snprintf(r_status, sizeof(r_status), "%s | %d/%d", E.syntax ? E.syntax->file_type : "no ft", E.cy + 1, E.num_rows);
    
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
            char * c = &E.row[file_row].render[E.col_offset];
            unsigned char * hl = &E.row[file_row].hl[E.col_offset];
            int current_color = -1;
            int j;
            for(j = 0; j < length; j++) {
                if(iscntrl(c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    ab_append(ab, "\x1b[7m", 4);
                    ab_append(ab, &sym, 1);
                    ab_append(ab, "\x1b[m", 3);
                    if(current_color != -1) {
                        char buf[16];
                        int c_len = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        ab_append(ab, buf, c_len);
                    }
                }
                else if(hl[j] == HL_NORMAL) {
                    if(current_color != -1) {
                        ab_append(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    ab_append(ab, &c[j], 1);
                }
                else {
                    int color = editor_syntax_to_color(hl[j]);
                    if(color != current_color) {
                        current_color = color;
                        char buf[16];
                        int c_length = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        ab_append(ab, buf, c_length);
                    }
                    ab_append(ab, &c[j], 1);
                }
            }
            ab_append(ab, "\x1b[39m", 5);
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

void editor_row_append_string(editor_row * row, char * s, size_t length) {
    row->chars = realloc(row->chars, row->size + length + 1);
    memcpy(&row->chars[row->size], s, length);
    row->size += length;
    row->chars[row->size] = '\0';
    editor_update_row(row);
    E.dirty++;
}

void editor_free_row(editor_row * row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void editor_delete_row(int at) {
    if(at < 0 || at >= E.num_rows) {
        return;
    }
    editor_free_row(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(editor_row) * (E.num_rows - at - 1));
    for(int j = at; j < E.num_rows - 1; j++) {
        E.row[j].index--;
    }
    
    E.num_rows--;
    E.dirty++;
}

void editor_row_delete_char(editor_row * row, int at) {
    if(at < 0 || at >= row->size) {
        return;
    }
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editor_update_row(row);
    E.dirty++;
}

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
    E.dirty++;
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

int rx_to_cx(editor_row * row, int rx) {
    int current_rx = 0;
    int cx;
    for(cx = 0; cx < row->size; cx++) {
        if(row->chars[cx] == '\t') {
            current_rx += (TAB_STOP - 1) - (current_rx % TAB_STOP);
        }
        current_rx++;
    }
    if(current_rx > rx) {
        return cx;
    }
    return cx;
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

    editor_update_syntax(row);
}

void editor_insert_row(int at, char * s, size_t length) {
    if(at < 0 || at > E.num_rows) {
        return;
    }
    E.row = realloc(E.row, sizeof(editor_row) * (E.num_rows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(editor_row) * (E.num_rows - at));
    for(int j = at + 1; j <= E.num_rows; j++) {
        E.row[j].index++;
    }

    E.row[at].index = at;

    E.row[at].size = length;
    E.row[at].chars = malloc(length + 1);
    memcpy(E.row[at].chars, s, length);
    E.row[at].chars[length] = '\0';

    E.row[at].r_size = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].hl_open_comment = 0;
    editor_update_row(&E.row[at]);

    E.num_rows++;
    E.dirty++;
}

/* file i/o */

void editor_save() {
    if(E.file_name == NULL) {
        E.file_name = editor_prompt("Save as: %s (ESC to cancel)", NULL);
        if(E.file_name == NULL) {
            editor_set_status("Save aborted");
            return;
        }
        editor_select_syntax_highlight();
    }

    int length;
    char * buffer = editor_rows_to_string(&length);

    // O_CREAT - creates new file if it doesn't exist
    // O_RDWR - open file for read and write
    // 0644 - gives owner permission to r/w, everyone else permission to r
    int fd = open(E.file_name, O_RDWR | O_CREAT, 0644);
    if(fd != -1) {
        if(ftruncate(fd, length) != -1) {
            if(write(fd, buffer, length) == length) {
                close(fd);
                free(buffer);
                E.dirty = 0;
                editor_set_status("%d bytes written to disk", length);
                return;
            }
        }
        close(fd);
    }
    free(buffer);
    editor_set_status("Can't save! I/O error: %s", strerror(errno));
}

char * editor_rows_to_string(int * buffer_length) {
    int total_length = 0;
    int j;
    for(j = 0; j < E.num_rows; j++) {
        // adds up individual lengths + 1 for new line char
        total_length += E.row[j].size + 1;
    }
    *buffer_length = total_length;

    char * buffer = malloc(total_length);
    char * p = buffer;
    for(j = 0; j < E.num_rows; j++) {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }

    return buffer;
}

void editor_open(char * file_name) {
    free(E.file_name);
    E.file_name = strdup(file_name);

    editor_select_syntax_highlight();

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
        editor_insert_row(E.num_rows, line, line_length);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
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
    E.dirty = 0;
    E.file_name = NULL;
    E.status_msg[0] = '\0';
    E.status_time = 0;
    E.syntax = NULL;

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

    editor_set_status("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    // reads user input until a q is seen
    while(1) {
        editor_refresh_screen();
        editor_process_keypress();
    }

    return 0;
}