// includes
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

//defines
#define CTRL_KEY(k) ((k) & 0x1f)    // strips top three bits
#define ABUF_INIT {NULL, 0}         // append_buffer constructor

// structure definitions
typedef struct editor_config {
    int screen_rows;
    int screen_cols;
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
char editor_read_key();         // wait for a keypress and return it
void enable_raw_mode();         // enable "raw-mode"
void disable_raw_mode();        // disable "raw-mode"
void die(const char *s);        // error function
int get_window_size(int * rows, int * cols);

// output
void editor_refresh_screen();
void editor_draw_rows();

// input
void editor_process_keypress(); // wait for a keypress and handle it

// init
void init_editor();