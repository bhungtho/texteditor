// includes
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

//defines
#define CTRL_KEY(k) ((k) & 0x1f)    // strips top three bits

// variables
struct termios orig_termios;

// function declarations
void enable_raw_mode();
void disable_raw_mode();
void die(const char *s);