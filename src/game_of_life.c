// The Game of Life - "world" variation.
// The board lives in a heap-allocated World with a one-cell "ghost" border, so
// neighbour counting never needs modulo maths: opposite edges are copied into the
// border before each tick to make the board wrap (toroidal). Generations are
// swapped by exchanging two pointers (ping-pong) instead of copying memory.

// Ask the standard library to expose POSIX helpers (isatty and /dev/tty access).
#define _POSIX_C_SOURCE 200809L

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Board is 80 columns wide and 25 rows tall, as the task requires.
#define W 80
#define H 25

// Speed is a delay between frames in milliseconds, adjustable at run time.
#define START_DELAY 120
#define MIN_DELAY 20
#define MAX_DELAY 1000
#define SPEED_STEP 20

// Two flat buffers, each with a ghost border, swapped every generation.
typedef struct {
    int h;
    int w;
    char* cur;
    char* nxt;
} World;

// Flat index of padded coordinate (pr, pc) inside a ghost-bordered buffer.
static int at(int w, int pr, int pc) { return pr * (w + 2) + pc; }

// Allocate both buffers (sized with the border). Returns 0 on success, -1 if not.
static int world_init(World* wd, int h, int w) {
    wd->h = h;
    wd->w = w;
    wd->cur = calloc((size_t)(h + 2) * (w + 2), sizeof(char));
    wd->nxt = calloc((size_t)(h + 2) * (w + 2), sizeof(char));
    if (wd->cur == NULL || wd->nxt == NULL) {
        return -1;
    }
    return 0;
}

// Release both buffers and leave the struct safe to free again.
static void world_free(World* wd) {
    free(wd->cur);
    free(wd->nxt);
    wd->cur = NULL;
    wd->nxt = NULL;
}

// Decide whether one input character means a living cell.
static int is_alive_char(int ch) {
    return ch == '*' || ch == 'O' || ch == 'o' || ch == 'X' || ch == 'x' || ch == '#' || ch == '@' ||
           ch == '+' || ch == '1';
}

// Read the starting board from standard input into the interior cells.
static void read_seed(World* wd) {
    for (int r = 0; r < wd->h; r++) {
        char line[256];
        if (fgets(line, (int)sizeof(line), stdin) == NULL) {
            break;
        }
        for (int c = 0; c < wd->w && line[c] != '\0' && line[c] != '\n'; c++) {
            if (is_alive_char((unsigned char)line[c])) {
                wd->cur[at(wd->w, r + 1, c + 1)] = 1;
            }
        }
        // If the row was longer than the buffer, skip the rest so rows stay aligned.
        if (strchr(line, '\n') == NULL) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {
            }
        }
    }
}

// With no input redirected, place a single glider so the screen is not empty.
static void load_default(World* wd) {
    int r = wd->h / 2;
    int c = wd->w / 2;
    wd->cur[at(wd->w, r, c + 1)] = 1;
    wd->cur[at(wd->w, r + 1, c + 2)] = 1;
    wd->cur[at(wd->w, r + 2, c)] = 1;
    wd->cur[at(wd->w, r + 2, c + 1)] = 1;
    wd->cur[at(wd->w, r + 2, c + 2)] = 1;
}

// Copy the real edges into the opposite ghost cells so the board behaves as a torus.
static void wrap_borders(World* wd) {
    int h = wd->h;
    int w = wd->w;
    char* g = wd->cur;
    for (int pr = 1; pr <= h; pr++) {
        g[at(w, pr, 0)] = g[at(w, pr, w)];
        g[at(w, pr, w + 1)] = g[at(w, pr, 1)];
    }
    for (int pc = 1; pc <= w; pc++) {
        g[at(w, 0, pc)] = g[at(w, h, pc)];
        g[at(w, h + 1, pc)] = g[at(w, 1, pc)];
    }
    g[at(w, 0, 0)] = g[at(w, h, w)];
    g[at(w, 0, w + 1)] = g[at(w, h, 1)];
    g[at(w, h + 1, 0)] = g[at(w, 1, w)];
    g[at(w, h + 1, w + 1)] = g[at(w, 1, 1)];
}

// Count living cells among the eight neighbours around padded coordinate (pr, pc).
static int count_neighbors(const World* wd, int pr, int pc) {
    const char* g = wd->cur;
    int w = wd->w;
    int live = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr != 0 || dc != 0) {
                live += g[at(w, pr + dr, pc + dc)];
            }
        }
    }
    return live;
}

// Build the next generation (B3/S23), then swap the two buffers by pointer.
static void step(World* wd) {
    int h = wd->h;
    int w = wd->w;
    wrap_borders(wd);
    for (int pr = 1; pr <= h; pr++) {
        for (int pc = 1; pc <= w; pc++) {
            int n = count_neighbors(wd, pr, pc);
            int alive = wd->cur[at(w, pr, pc)] != 0;
            int born = (alive && (n == 2 || n == 3)) || (!alive && n == 3);
            wd->nxt[at(w, pr, pc)] = (char)(born ? 1 : 0);
        }
    }
    char* tmp = wd->cur;
    wd->cur = wd->nxt;
    wd->nxt = tmp;
}

// Count how many interior cells are alive.
static int population(const World* wd) {
    int count = 0;
    for (int pr = 1; pr <= wd->h; pr++) {
        for (int pc = 1; pc <= wd->w; pc++) {
            if (wd->cur[at(wd->w, pr, pc)] != 0) {
                count++;
            }
        }
    }
    return count;
}

// Draw the board on screen at the given top-left offset, one character per cell.
static void draw_cells(const World* wd, int oy, int ox) {
    for (int r = 0; r < wd->h; r++) {
        for (int c = 0; c < wd->w; c++) {
            char ch = wd->cur[at(wd->w, r + 1, c + 1)] ? 'O' : ' ';
            mvaddch(oy + r, ox + c, (chtype)ch);
        }
    }
}

// Draw a rectangular frame around an h x w board whose top-left corner is (oy, ox).
static void draw_frame(int oy, int ox, int h, int w) {
    mvhline(oy - 1, ox, ACS_HLINE, w);
    mvhline(oy + h, ox, ACS_HLINE, w);
    mvvline(oy, ox - 1, ACS_VLINE, h);
    mvvline(oy, ox + w, ACS_VLINE, h);
    mvaddch(oy - 1, ox - 1, ACS_ULCORNER);
    mvaddch(oy - 1, ox + w, ACS_URCORNER);
    mvaddch(oy + h, ox - 1, ACS_LLCORNER);
    mvaddch(oy + h, ox + w, ACS_LRCORNER);
}

// React to a key press: adjust the frame delay, or report a request to quit.
static int apply_key(int ch, int* delay) {
    if (ch == 'a' || ch == 'A') {
        *delay -= SPEED_STEP;
        if (*delay < MIN_DELAY) {
            *delay = MIN_DELAY;
        }
    } else if (ch == 'z' || ch == 'Z') {
        *delay += SPEED_STEP;
        if (*delay > MAX_DELAY) {
            *delay = MAX_DELAY;
        }
    } else if (ch == ' ') {
        return 1;
    }
    return 0;
}

// Interactive loop: an optional frame (when the terminal is big enough) plus stats.
static void run_ncurses(World* wd) {
    int delay = START_DELAY;
    int gen = 0;
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    // Frame the board only when the terminal is at least the board size plus a border.
    int framed = COLS >= wd->w + 2 && LINES >= wd->h + 2;
    int oy = framed ? 1 : 0;
    int ox = framed ? 1 : 0;
    while (1) {
        timeout(delay);
        if (apply_key(getch(), &delay)) {
            break;
        }
        erase();
        if (framed) {
            draw_frame(oy, ox, wd->h, wd->w);
        }
        draw_cells(wd, oy, ox);
        if (LINES > wd->h + 2) {
            mvprintw(LINES - 1, 0, "gen:%-5d pop:%-4d delay:%dms  A:+ Z:- Space:quit", gen, population(wd),
                     delay);
        }
        refresh();
        step(wd);
        gen++;
    }
    curs_set(1);
    endwin();
}

// Advance the simulation with no screen output (used by the headless test mode).
static void run_headless(World* wd, int frames) {
    for (int i = 0; i < frames; i++) {
        step(wd);
    }
    printf("frames=%d population=%d\n", frames, population(wd));
}

int main(void) {
    World wd;
    if (world_init(&wd, H, W) != 0) {
        world_free(&wd);
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    // Headless test mode: if GOL_FRAMES is set, run that many ticks with no screen.
    const char* frames_env = getenv("GOL_FRAMES");
    int frames = frames_env != NULL ? atoi(frames_env) : -1;
    // Seed from a redirected file; on a bare terminal fall back to a built-in glider.
    int redirected = !isatty(STDIN_FILENO);
    if (redirected) {
        read_seed(&wd);
    } else {
        load_default(&wd);
    }
    if (frames >= 0) {
        run_headless(&wd, frames);
        world_free(&wd);
        return 0;
    }
    // Only when the seed came from a redirect do we reopen the terminal for keys.
    if (redirected && freopen("/dev/tty", "r", stdin) == NULL) {
        fprintf(stderr, "cannot open terminal for input\n");
        world_free(&wd);
        return 1;
    }
    run_ncurses(&wd);
    world_free(&wd);
    return 0;
}
