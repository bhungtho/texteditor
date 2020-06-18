/* defines */

#define QUIT_TIMES 3
#define VERSION "0.0.1"
#define TAB_STOP 8
#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))
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
#include <fcntl.h>

/* enums */

// key presses
enum editor_key {
    BACKSPACE = 127,
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

// highlights
enum editor_highlight {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};

/* structure definitions */

// one row of text
typedef struct editor_row {
    int size;           // size of the row
    int r_size;         // size of the render
    char * chars;       // text
    char * render;      // render text
    unsigned char * hl; // highlight of the line
} editor_row;

// editor variables
typedef struct editor_syntax {
    char * file_type;       // name of the filetype that will be displayed
    char ** file_match;     // array of file-type patterns
    char ** keywords;       // array of language-specific keywords
    char * singleline_comment_start;
    int flags;
} editor_syntax;

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
    int dirty;                      // dirty flag
    char * file_name;               // name of the file
    char status_msg[80];            // status message
    time_t status_time;             // status time
    editor_syntax * syntax;
    struct termios orig_termios;    // holds flags for the terminal
} editor_config;

// to-add to editor
typedef struct append_buffer {
    char * b;       // actual buffer
    int len;        // length of buffer
} append_buffer;

/* variables */
char * C_HL_extensions[] = { ".c", ".h", ".cpp", NULL};
char * C_HL_keywords[] = {"switch", "if", "while", "for", "break", "continue", "return", "else", "struct", "union", "typedef", "static", "enum", "class", "case", "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|", "void|", NULL};
editor_config E;    // our editor
editor_syntax HLDB[] = {{"c", C_HL_extensions, C_HL_keywords, "//", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},};     // HIGHLIGHT DATABASE

/* function declarations */

// syntax highlighting
void editor_select_syntax_highlight();
int editor_syntax_to_color(int hl);
void editor_update_syntax(editor_row * row);

// find
void editor_find_callback(char * query, int key);
void editor_find();

// editor operations
void editor_insert_new_line();
void editor_delete_char();
void editor_insert_char(int c);

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
char * editor_prompt(char * prompt, void (* callback)(char *, int));
void editor_move_cursor(int key);   // move the cursor on the screen
void editor_process_keypress();     // wait for a keypress and handle it

// row operations
void editor_row_append_string(editor_row * row, char * s, size_t length);
void editor_free_row(editor_row * row);
void editor_delete_row(int at);
void editor_row_delete_char(editor_row * row, int at);
void editor_row_insert_char(editor_row * row, int at, int c);
int cx_to_rx(editor_row * row, int cx);           // converts cx to rx
int rx_to_cx(editor_row * row, int rx);
void editor_update_row(editor_row * row);         // updates the specified row
void editor_insert_row(int at, char * s, size_t length);  // appends the row to the screen

// file i/o
void editor_save();                                 // saves the file
void editor_open();                                 // opens the terminal to the editor
char * editor_rows_to_string(int * buffer_length);  // converts array of editor_row into a single string

// init
void init_editor(); // initializes the editor