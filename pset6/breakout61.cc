#include "board.hh"
#include "helpers.hh"
#include <unistd.h>
#include <sys/time.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <csignal>
#include <cassert>
#include <atomic>
#include <thread>
#include <random>
#include <deque>
#include <functional>


// breakout board
pong_board* main_board;

// delay between moves, in microseconds
static unsigned long delay;
// warp delay, in microseconds
static unsigned long warp_delay = 200000;

// number of running threads
static std::atomic<long> nrunning;


// THREADS

// ball_thread(b)
//    Move a ball until it falls off the board. Then destroy the
//    ball and exit.

void ball_thread(pong_ball* b) {
    ++nrunning;

    while (true) {
        int mval = b->move();
        if (mval > 0) {
            // ball successfully moved; wait `delay` to move it again
            if (delay > 0) {
                usleep(delay);
            }
        } else if (mval < 0) {
            // ball destroyed
            break;
        }
    }

    delete b;
    --nrunning;
}


// warp_thread(w)
//    Handle a warp tunnel. Must behave as follows:
//
//    1. Wait for a ball to enter this tunnel
//       (see `warp_thread::accept_ball()`).
//    2. Hide the ball for `warp_delay` microseconds.
//    3. Reveal the ball at position `warp->x,warp->y` (once that
//       position is available) and send it on its way.
//    4. Return to step 1.
//
//    The handout code is not thread-safe. Remove the `usleep(0)` functions
//    and it doesn’t work at all!

void warp_thread(pong_warp* w) {
    pong_cell& cdest = w->board.cell(w->x, w->y);
    while (true) {
        // wait for a ball to arrive
        while (!w->ball) {
            usleep(0);
        }
        pong_ball* b = w->ball;
        w->ball = nullptr;

        // ball stays in the warp tunnel for `warp_delay` usec
        usleep(warp_delay);

        // then it appears on the destination cell
        assert(!cdest.ball);
        cdest.ball = b;
        b->x = w->x;
        b->y = w->y;
        b->stopped = false;
    }
}


// paddle_thread(board, x, y, width)
//    Thread to move the paddle, which starts at position (`px`, `py`) and has
//    width `pw`.
//
//    PART 2: Your code here! Our handout code just moves the paddle back &
//    forth.

void paddle_thread(pong_board* b, int px, int py, int pw) {
    int dx = 1;
    while (true) {
        if (px + dx >= 0 && px + dx + pw <= b->width) {
            px += dx;
        } else {
            // hit edge of screen; reverse
            dx = -dx;
        }

        // represent paddle position as cell_paddle vs. cell_empty
        for (int x = 0; x < b->width; ++x) {
            if (x >= px && x < px + pw) {
                b->cell(x, py).type = cell_paddle;
            } else {
                b->cell(x, py).type = cell_empty;
            }
        }

        usleep(delay / 2);
    }
}


// MAIN

// signal handlers
static bool is_tty;
void summary_handler(int);
__attribute__((no_sanitize("thread")))
void print_handler(int);

// usage()
//    Explain how breakout61 should be run.
static void usage() {
    fprintf(stderr, "\
Usage: ./breakout61 [-P] [-1] [-w WIDTH] [-h HEIGHT] [-b NBALLS] [-s NSTICKY]\n\
                    [-W NWARP] [-B NBRICKS] [-d MOVEPAUSE] [-p PRINTTIMER]\n");
    exit(1);
}

int main(int argc, char** argv) {
    // print information on receiving a signal
    {
        is_tty = isatty(STDOUT_FILENO);
        struct sigaction sa;
        sa.sa_handler = print_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        int r = sigaction(SIGUSR1, &sa, nullptr);
        assert(r == 0);
        r = sigaction(SIGALRM, &sa, nullptr);
        assert(r == 0);
        sa.sa_handler = summary_handler;
        r = sigaction(SIGUSR2, &sa, nullptr);
        assert(r == 0);
    }

    // parse arguments and check size invariants
    int width = 100, height = 31, nballs = 24, nsticky = 12,
        nwarps = 0, nbricks = -1;
    long print_interval = 0;
    bool single_threaded = false, paddle = false;
    int ch;
    while ((ch = getopt(argc, argv, "w:h:b:B:s:d:p:W:j:1P")) != -1) {
        if (ch == 'w' && is_integer_string(optarg)) {
            width = strtol(optarg, nullptr, 10);
        } else if (ch == 'h' && is_integer_string(optarg)) {
            height = strtol(optarg, nullptr, 10);
        } else if (ch == 'b' && is_integer_string(optarg)) {
            nballs = strtol(optarg, nullptr, 10);
        } else if (ch == 's' && is_integer_string(optarg)) {
            nsticky = strtol(optarg, nullptr, 10);
        } else if (ch == 'W' && is_integer_string(optarg)) {
            nwarps = strtol(optarg, nullptr, 10);
        } else if (ch == 'B' && is_integer_string(optarg)) {
            nbricks = strtol(optarg, nullptr, 10);
        } else if (ch == 'd' && is_real_string(optarg)) {
            delay = (unsigned long) (strtod(optarg, nullptr) * 1000000);
        } else if (ch == 'p' && is_real_string(optarg)) {
            print_interval = (long) (strtod(optarg, nullptr) * 1000000);
        } else if (ch == 'P') {
            paddle = true;
        } else if (ch == '1') {
            single_threaded = true;
        } else {
            usage();
        }
    }
    if (nbricks < 0) {
        nbricks = height / 3;
    }
    if (optind != argc
        || width < 2
        || height < 2
        || nballs + nsticky + nwarps + width * nbricks >= width * (height - 2)
        || width > 1024
        || nwarps % 2 != 0
        || nballs == 0) {
        usage();
    }

    // set up interval timer to print board
    if (print_interval > 0) {
        struct itimerval it;
        it.it_interval.tv_sec = print_interval / 1000000;
        it.it_interval.tv_usec = print_interval % 1000000;
        it.it_value = it.it_interval;
        int r = setitimer(ITIMER_REAL, &it, nullptr);
        assert(r == 0);
    }

    // create pong board
    pong_board board(width, height);
    main_board = &board;

    // place bricks
    for (int n = 0; n < nbricks; ++n) {
        for (int x = 0; x < width; ++x) {
            board.cell(x, n).type = cell_obstacle;
            board.cell(x, n).strength = (nbricks - n - 1) / 2 + 1;
        }
    }

    // place paddle
    if (paddle) {
        for (int x = 0; x < width; ++x) {
            board.cell(x, height - 1).type = cell_trash;
        }
        for (int x = 0; x < 8 && x < width; ++x) {
            board.cell(x, height - 2).type = cell_paddle;
        }
    }

    // place sticky locations
    for (int n = 0; n < nsticky; ++n) {
        int x, y;
        do {
            x = random_int(0, width - 1);
            y = random_int(0, height - 3);
        } while (board.cell(x, y).type != cell_empty);
        board.cell(x, y).type = cell_sticky;
    }

    // place warp pairs
    for (int n = 0; n < nwarps; n += 2) {
        pong_warp* w1 = new pong_warp(board);
        pong_warp* w2 = new pong_warp(board);

        do {
            w1->x = random_int(0, width - 1);
            w1->y = random_int(0, height - 3);
        } while (board.cell(w1->x, w1->y).type != cell_empty);
        board.cell(w1->x, w1->y).type = cell_warp;

        do {
            w2->x = random_int(0, width - 1);
            w2->y = random_int(0, height - 3);
        } while (board.cell(w2->x, w2->y).type != cell_empty);
        board.cell(w2->x, w2->y).type = cell_warp;

        // Each warp cell contains a pointer to the other end of the warp.
        board.cell(w1->x, w1->y).warp = w2;
        board.cell(w2->x, w2->y).warp = w1;

        board.warps.push_back(w1);
        board.warps.push_back(w2);
    }

    // place balls
    std::vector<pong_ball*> balls;
    for (int n = 0; n < nballs; ++n) {
        pong_ball* b = new pong_ball(board);
        do {
            b->x = random_int(0, width - 1);
            b->y = random_int(0, height - 3);
        } while (board.cell(b->x, b->y).type > cell_sticky
                 || board.cell(b->x, b->y).ball);
        b->dx = random_int(0, 1) ? 1 : -1;
        b->dy = random_int(0, 1) ? 1 : -1;
        board.cell(b->x, b->y).ball = b;
        balls.push_back(b);
    }

    if (!single_threaded) {
        // create ball threads
        for (auto b : balls) {
            std::thread t(ball_thread, b);
            t.detach();
        }

        // create warp threads
        for (auto w : board.warps) {
            std::thread t(warp_thread, w);
            t.detach();
        }

        // create paddle thread
        if (paddle) {
            std::thread t(paddle_thread, main_board, 0, height - 2,
                          std::min(8, width));
            t.detach();
        }

        // main thread blocks forever
        while (true) {
            select(0, nullptr, nullptr, nullptr, nullptr);
        }

    } else {
        // single threaded mode: one thread runs all balls
        // (single threaded mode does not handle warps)
        assert(nwarps == 0);
        while (true) {
            for (auto b : balls) {
                b->move();
            }
            if (delay) {
                usleep(delay);
            }
        }
    }
}


// SIGNAL HANDLERS

// summary_handler
//    Runs when `SIGUSR2` is received; prints out summary statistics for the
//    board to standard output.
void summary_handler(int) {
    char buf[BUFSIZ];
    ssize_t nw = 0;
    if (main_board) {
        simple_printer pr(buf, sizeof(buf));
        pr << nrunning << " balls, "
           << main_board->ncollisions << " collisions\n"
           << spflush(STDOUT_FILENO);
    }
    assert(nw >= 0);
}

// signal_handler
//    Runs when `SIGUSR1` is received; prints out the current state of the
//    board to standard output.
//
//    NOTE: This function accesses the board in a thread-unsafe way. There is no
//    easy way to fix this and you aren't expected to fix it.
__attribute__((no_sanitize("thread")))
void print_handler(int) {
    static const unsigned char obstacle_colors[16] = {
        227, 46, 214, 160, 100, 101, 136, 137,
        138, 173, 174, 175, 210, 211, 212, 213
    };
    char buf[8192];
    simple_printer sp(buf, sizeof(buf));
    if (main_board) {
        if (is_tty) {
            // clear screen
            sp << "\x1B[H\x1B[J" << spflush(STDOUT_FILENO);
        }

        summary_handler(0);

        for (int y = 0; y < main_board->height; ++y) {
            sp << (is_tty ? "\x1B[m" : "");
            for (int x = 0; x < main_board->width; ++x) {
                pong_cell& c = main_board->cell(x, y);
                if (auto b = c.ball) {
                    int color = (reinterpret_cast<uintptr_t>(b) / 131) % 6;
                    if (is_tty && c.type == cell_sticky) {
                        sp.snprintf("\x1B[%dmO\x1B[m", 31 + color);
                    } else if (is_tty) {
                        sp.snprintf("\x1B[1;%dmO\x1B[m", 31 + color);
                    } else {
                        sp << 'O';
                    }
                } else if (c.type == cell_empty) {
                    sp << '.';
                } else if (c.type == cell_sticky) {
                    sp << (is_tty ? "\x1B[37m_\x1B[m" : "_");
                } else if (c.type == cell_obstacle) {
                    if (c.strength == 0) {
                        sp << (is_tty ? "\x1B[48;5;28mX\x1B[m" : "X");
                    } else if (is_tty) {
                        sp.snprintf("\x1B[48;5;%dm%c\x1B[m",
                            obstacle_colors[std::min(c.strength, 16) - 1],
                            c.strength > 9 ? '%' : '0' + c.strength);
                    } else {
                        sp << (char) (c.strength > 9 ? '%' : '0' + c.strength);
                    }
                } else if (c.type == cell_paddle) {
                    sp << (is_tty ? "\x1B[97;104m=\x1B[m" : "=");
                } else if (c.type == cell_warp) {
                    sp << (is_tty ? "\x1B[97;45mW\x1B[m" : "W");
                } else if (c.type == cell_trash) {
                    sp << (is_tty ? "\x1B[32;40mX\x1B[m" : "X");
                } else {
                    sp << '?';
                }
            }
            sp << '\n' << spflush(STDOUT_FILENO);
        }
        sp << '\n' << spflush(STDOUT_FILENO);
    }
}
