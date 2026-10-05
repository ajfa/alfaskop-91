/* ncurses.h stand-in for the MSYS2 build of the 3705 emulator: curses is only used by the optional
   line status panel, which gives up when has_colors() is false, so every call does nothing. */
#ifndef SHIM_NCURSES_H
#define SHIM_NCURSES_H
#include <stdio.h>
typedef struct shim_screen { int unused; } SCREEN;
typedef struct shim_window { int unused; } WINDOW;
static WINDOW shim_stdscr;
#define stdscr (&shim_stdscr)
#define TRUE 1
#define FALSE 0
#define ERR (-1)
#define KEY_HOME 0406
#define COLOR_BLACK 0
#define COLOR_RED 1
#define COLOR_GREEN 2
#define COLOR_YELLOW 3
#define COLOR_BLUE 4
#define COLOR_MAGENTA 5
#define COLOR_CYAN 6
#define COLOR_WHITE 7
#define COLOR_PAIR(n) ((n) << 8)
static inline int refresh(void) { fflush(stdout); return 0; }
static inline int getch(void) { return ERR; }
static inline int endwin(void) { return 0; }
static inline SCREEN *newterm(const char *t, FILE *o, FILE *i) { (void)t; (void)o; (void)i; return NULL; }
static inline SCREEN *set_term(SCREEN *s) { return s; }
static inline int curs_set(int v) { (void)v; return 0; }
static inline int has_colors(void) { return FALSE; }
static inline int start_color(void) { return 0; }
static inline int init_color(short c, short r, short g, short b) { (void)c; (void)r; (void)g; (void)b; return 0; }
static inline int init_pair(short p, short f, short b) { (void)p; (void)f; (void)b; return 0; }
static inline int noecho(void) { return 0; }
static inline int nodelay(WINDOW *w, int b) { (void)w; (void)b; return 0; }
static inline int keypad(WINDOW *w, int b) { (void)w; (void)b; return 0; }
static inline int wmove(WINDOW *w, int y, int x) { (void)w; (void)y; (void)x; return 0; }
static inline int attron(int a) { (void)a; return 0; }
static inline int attroff(int a) { (void)a; return 0; }
static inline int printw(const char *f, ...) { (void)f; return 0; }
static inline int mvprintw(int y, int x, const char *f, ...) { (void)y; (void)x; (void)f; return 0; }
static inline int clear(void) { return 0; }
#endif
