/* defines */

#define VERSION "0.0.1"
#define TAB_STOP 8
#define CTRL_KEY(k) ((k) & 0x1f)    // strips top three bits
#define ABUF_INIT {NULL, 0}         // append_buffer constructor
// feature test macros
//#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#define _SVID_SOURCE

/* includes */

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>

/* enums */

// key presses
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

/* structure definitions */

// one row of text
typedef struct editor_row {
    int size;       // size of the row
    int r_size;     // size of the render
    char * chars;   // text
    char * render;  // render text
} editor_row;

// editor variables
typedef struct editor_config {
    int cx;                         // cursor x location
    int cy;                         // cursor y location
    int rx;                         // index into render
    int row_offset;                 // what row the user is on
    int col_offset;                 // column offset
    int screen_rows;                // number of rows on the screen
    int screen_cols;                // number of columns of the screen
    int num_rows;                   // number of rows in the file
    editor_row  * row;              // array of editor_rows that hold the text
    char * file_name;               // name of the file
    char status_msg[80];            // status message
    time_t status_time;             // status time
    struct termios orig_termios;    // holds flags for the terminal
} editor_config;

// to-add to editor
typedef struct append_buffer {
    char * b;       // actual buffer
    int len;        // length of buffer
} append_buffer;

/* variables */

editor_config E;    // our editor

/* function declarations */

// append buffer
void ab_append(append_buffer * ab, const char * s, int len);    // add to our buffer
void ab_free(append_buffer * ab);                               // free buffer

// terminal
int editor_read_key();          // wait for a keypress and return it
void enable_raw_mode();         // enable "raw-mode"
void disable_raw_mode();        // disable "raw-mode"
void die(const char *s);        // error function
int get_window_size(int * rows, int * cols);

// output
void editor_set_status(const char * fmt, ...);      // set status bar message
void editor_draw_message_bar(append_buffer * ab);   // draw message bar on screen
void editor_draw_status_bar(append_buffer * ab);    // draw status bar on screen
void editor_scroll();                               // scroll the screen
void editor_refresh_screen();                       // refresh the screen
void editor_draw_rows();                            // draw the rows on the screen

// input
void editor_move_cursor(int key);   // move the cursor on the screen
void editor_process_keypress();     // wait for a keypress and handle it

// row operations
int cx_to_rx(editor_row * row, int cx);           // converts cx to rx
void editor_update_row(editor_row * row);         // updates the specified row
void editor_append_row(char * s, size_t length);  // appends the row to the screen

// file i/o
void editor_open(); // opens the terminal to the editor

// init
void init_editor(); // initializes the editor