// includes
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>

//defines
#define VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)    // strips top three bits
#define ABUF_INIT {NULL, 0}         // append_buffer constructor
// feature test macros
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

// enums
enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

// structure definitions
typedef struct editor_row {
    int size;
    char * chars;
} editor_row;

typedef struct editor_config {
    int cx;
    int cy;
    int row_offset;
    int col_offset;
    int screen_rows;
    int screen_cols;
    int num_rows;
    editor_row  * row;
    struct termios orig_termios;
} editor_config;

typedef struct append_buffer {
    char * b;
    int len;
} append_buffer;

// variables
editor_config E;

// function declarations

// append buffer
void ab_append(append_buffer * ab, const char * s, int len);
void ab_free(append_buffer * ab);

// terminal
int editor_read_key();         // wait for a keypress and return it
void enable_raw_mode();         // enable "raw-mode"
void disable_raw_mode();        // disable "raw-mode"
void die(const char *s);        // error function
int get_window_size(int * rows, int * cols);

// output
void editor_scroll();
void editor_refresh_screen();
void editor_draw_rows();

// input
void editor_move_cursor(int key);
void editor_process_keypress(); // wait for a keypress and handle it

// row operations
void editor_append_row(char * s, size_t length);

// file i/o
void editor_open();

// init
void init_editor();