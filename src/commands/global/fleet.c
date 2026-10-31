// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file fleet.c
 * @brief Fleet — guess enemy ship positions on a 10×10 grid.
 *
 * You and the CPU each have 5 ships placed on a 10×10 ocean.
 * Take turns firing at coordinates. First to sink all ships wins.
 * Controls: arrows to aim cursor, space/enter to fire, q to quit.
 *
 * Ships: Carrier(5), Frigate(4), Cruiser(3), Submarine(3), Destroyer(2)
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_toolbar.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "lib/bp_args/bp_cmd.h"
#include "game_engine.h"
#include "fleet.h"

#define BS_SIZE 10
#define BS_SHIPS 5

static const char* const usage[] = {
    "fleet",
    "Launch Fleet:%s fleet",
    "",
    "Sink all 5 enemy ships on a 10x10 grid.",
    "arrows=aim  space/enter=fire  r=restart  q=quit",
};

const bp_command_def_t fleet_def = {
    .name = "fleet",
    .description = T_HELP_FLEET,
    .usage = usage,
    .usage_count = count_of(usage),
};

// Cell states
#define CELL_WATER  0
#define CELL_SHIP   1
#define CELL_HIT    2
#define CELL_MISS   3

typedef struct {
    uint8_t player_grid[BS_SIZE][BS_SIZE];
    uint8_t enemy_grid[BS_SIZE][BS_SIZE];
    uint8_t player_shots[BS_SIZE][BS_SIZE];
    uint8_t enemy_shots[BS_SIZE][BS_SIZE];
    int cur_r;
    int cur_c;
} fleet_state_t;

static const int ship_sizes[BS_SHIPS] = { 5, 4, 3, 3, 2 };
static const char* const ship_names[BS_SHIPS] = {
    "Carrier", "Frigate", "Cruiser", "Submarine", "Destroyer"
};

static bool place_ship(uint8_t grid[BS_SIZE][BS_SIZE], int size) {
    for (int attempts = 0; attempts < 200; attempts++) {
        int dir = game_rng_next() % 2; // 0=horiz, 1=vert
        int r = game_rng_next() % BS_SIZE;
        int c = game_rng_next() % BS_SIZE;
        bool ok = true;
        for (int i = 0; i < size; i++) {
            int nr = r + (dir ? i : 0);
            int nc = c + (dir ? 0 : i);
            if (nr >= BS_SIZE || nc >= BS_SIZE || grid[nr][nc] != CELL_WATER) {
                ok = false;
                break;
            }
        }
        if (ok) {
            for (int i = 0; i < size; i++) {
                int nr = r + (dir ? i : 0);
                int nc = c + (dir ? 0 : i);
                grid[nr][nc] = CELL_SHIP;
            }
            return true;
        }
    }
    return false;
}

static void place_all_ships(uint8_t grid[BS_SIZE][BS_SIZE]) {
    for (int s = 0; s < BS_SHIPS; s++) {
        place_ship(grid, ship_sizes[s]);
    }
}

static int count_remaining(uint8_t grid[BS_SIZE][BS_SIZE], uint8_t shots[BS_SIZE][BS_SIZE]) {
    int count = 0;
    for (int r = 0; r < BS_SIZE; r++)
        for (int c = 0; c < BS_SIZE; c++)
            if (grid[r][c] == CELL_SHIP && shots[r][c] != CELL_HIT)
                count++;
    return count;
}

// CPU targeting: hunt mode (random) — simple but effective
static void cpu_fire(fleet_state_t* gs) {
    for (int attempts = 0; attempts < 200; attempts++) {
        int r = game_rng_next() % BS_SIZE;
        int c = game_rng_next() % BS_SIZE;
        if (gs->enemy_shots[r][c] == CELL_WATER) {
            if (gs->player_grid[r][c] == CELL_SHIP)
                gs->enemy_shots[r][c] = CELL_HIT;
            else
                gs->enemy_shots[r][c] = CELL_MISS;
            return;
        }
    }
    // Fallback: first empty cell
    for (int r = 0; r < BS_SIZE; r++)
        for (int c = 0; c < BS_SIZE; c++)
            if (gs->enemy_shots[r][c] == CELL_WATER) {
                if (gs->player_grid[r][c] == CELL_SHIP)
                    gs->enemy_shots[r][c] = CELL_HIT;
                else
                    gs->enemy_shots[r][c] = CELL_MISS;
                return;
            }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void draw_grid_header(int row, int col, const char* title) {
    ui_term_cursor_position(row, col);
    printf("\x1b[1m%s\x1b[0m", title);
    ui_term_cursor_position(row + 1, col);
    printf("  ");
    for (int c = 0; c < BS_SIZE; c++) printf("%c ", 'A' + c);
}

static void draw_grid(fleet_state_t* gs, int row, int col, uint8_t grid[BS_SIZE][BS_SIZE],
                      uint8_t shots[BS_SIZE][BS_SIZE], bool show_ships, bool show_cursor) {
    bool color = system_config.terminal_ansi_color;
    const char* rst = color ? "\x1b[0m" : "";

    for (int r = 0; r < BS_SIZE; r++) {
        ui_term_cursor_position(row + 2 + r, col);
        printf("%c ", '0' + r);
        for (int c = 0; c < BS_SIZE; c++) {
            char ch;
            if (shots[r][c] == CELL_HIT) {
                if (color) printf("\x1b[1;31m");
                ch = 'X';
            } else if (shots[r][c] == CELL_MISS) {
                if (color) printf("\x1b[36m");
                ch = '~';
            } else if (show_ships && grid[r][c] == CELL_SHIP) {
                if (color) printf("\x1b[1;32m");
                ch = '#';
            } else {
                if (color) printf("\x1b[34m");
                ch = '.';
            }

            if (show_cursor && r == gs->cur_r && c == gs->cur_c) {
                printf("\x1b[7m%c%s ", ch, rst);
            } else {
                printf("%c%s ", ch, rst);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void fleet_handler(struct command_result* res) {
    if (bp_cmd_help_check(&fleet_def, res->help_flag)) return;

    int term_cols = system_config.terminal_ansi_columns;
    (void)term_cols;

    game_screen_enter(0);

    game_rng_seed();

    fleet_state_t gs = {0};
    bool quit = false;

    while (!quit) {
        memset(gs.player_grid, CELL_WATER, sizeof(gs.player_grid));
        memset(gs.enemy_grid, CELL_WATER, sizeof(gs.enemy_grid));
        memset(gs.player_shots, CELL_WATER, sizeof(gs.player_shots));
        memset(gs.enemy_shots, CELL_WATER, sizeof(gs.enemy_shots));

        place_all_ships(gs.player_grid);
        place_all_ships(gs.enemy_grid);

        gs.cur_r = 0;
        gs.cur_c = 0;

        int sr = 2;
        int sc_enemy = 3;
        int sc_player = sc_enemy + 24;
        const char* status = "Your turn: aim and fire!";
        bool game_active = true;

        printf("\x1b[2J");
        while (game_active && !quit) {
            tx_fifo_wait_drain();

            ui_term_cursor_position(1, sc_enemy);
            printf("\x1b[1mFleet\x1b[0m  arrows=aim  space=fire  r=new  q=quit");

            draw_grid_header(sr, sc_enemy, "Enemy Ocean");
            draw_grid(&gs, sr, sc_enemy, gs.enemy_grid, gs.player_shots, false, true);

            draw_grid_header(sr, sc_player, "Your Fleet");
            draw_grid(&gs, sr, sc_player, gs.player_grid, gs.enemy_shots, true, false);

            ui_term_cursor_position(sr + BS_SIZE + 3, sc_enemy);
            printf("%s\x1b[K", status);

            int enemy_remaining = count_remaining(gs.enemy_grid, gs.player_shots);
            int player_remaining = count_remaining(gs.player_grid, gs.enemy_shots);
            ui_term_cursor_position(sr + BS_SIZE + 4, sc_enemy);
            printf("Enemy ships: %d cells   Your ships: %d cells", enemy_remaining, player_remaining);

            // Player input
            bool fired = false;
            while (!fired && !quit) {
                char c;
                if (rx_fifo_try_get(&c)) {
                    if (c == 'q' || c == 'Q') { quit = true; break; }
                    if (c == 'r' || c == 'R') { game_active = false; break; }

                    if (c == 0x1b) {
                        char s1, s2;
                        busy_wait_ms(2);
                        if (rx_fifo_try_get(&s1) && s1 == '[') {
                            busy_wait_ms(2);
                            if (rx_fifo_try_get(&s2)) {
                                if (s2 == 'A' && gs.cur_r > 0) gs.cur_r--;
                                else if (s2 == 'B' && gs.cur_r < BS_SIZE - 1) gs.cur_r++;
                                else if (s2 == 'D' && gs.cur_c > 0) gs.cur_c--;
                                else if (s2 == 'C' && gs.cur_c < BS_SIZE - 1) gs.cur_c++;
                                // Quick redraw of enemy grid
                                tx_fifo_wait_drain();
                                draw_grid(&gs, sr, sc_enemy, gs.enemy_grid, gs.player_shots, false, true);
                            }
                        }
                        continue;
                    }

                    if (c == ' ' || c == '\r' || c == '\n') {
                        if (gs.player_shots[gs.cur_r][gs.cur_c] != CELL_WATER) {
                            status = "Already fired there!";
                            tx_fifo_wait_drain();
                            ui_term_cursor_position(sr + BS_SIZE + 3, sc_enemy);
                            ui_term_erase_line();
                            printf("%s", status);
                            continue;
                        }
                        // Fire!
                        if (gs.enemy_grid[gs.cur_r][gs.cur_c] == CELL_SHIP) {
                            gs.player_shots[gs.cur_r][gs.cur_c] = CELL_HIT;
                            status = "HIT!";
                        } else {
                            gs.player_shots[gs.cur_r][gs.cur_c] = CELL_MISS;
                            status = "Miss...";
                        }
                        fired = true;
                    }
                }
                busy_wait_ms(10);
            }
            if (quit || !game_active) break;

            // Check player win
            if (count_remaining(gs.enemy_grid, gs.player_shots) == 0) {
                tx_fifo_wait_drain();
                printf("\x1b[2J");
                draw_grid_header(sr, sc_enemy, "Enemy Ocean");
                draw_grid(&gs, sr, sc_enemy, gs.enemy_grid, gs.player_shots, true, false);
                draw_grid_header(sr, sc_player, "Your Fleet");
                draw_grid(&gs, sr, sc_player, gs.player_grid, gs.enemy_shots, true, false);
                ui_term_cursor_position(sr + BS_SIZE + 3, sc_enemy);
                printf("\x1b[1;32mYou sank all enemy ships! r=again q=quit\x1b[0m");
                bool decided = false;
                while (!decided) {
                    char ch;
                    if (rx_fifo_try_get(&ch)) {
                        if (ch == 'q' || ch == 'Q') { quit = true; decided = true; }
                        if (ch == 'r' || ch == 'R') { decided = true; }
                    }
                    busy_wait_ms(10);
                }
                game_active = false;
                continue;
            }

            // CPU turn
            cpu_fire(&gs);
            if (count_remaining(gs.player_grid, gs.enemy_shots) == 0) {
                tx_fifo_wait_drain();
                printf("\x1b[2J");
                draw_grid_header(sr, sc_enemy, "Enemy Ocean");
                draw_grid(&gs, sr, sc_enemy, gs.enemy_grid, gs.player_shots, false, false);
                draw_grid_header(sr, sc_player, "Your Fleet");
                draw_grid(&gs, sr, sc_player, gs.player_grid, gs.enemy_shots, true, false);
                ui_term_cursor_position(sr + BS_SIZE + 3, sc_enemy);
                printf("\x1b[1;31mCPU sank all your ships! r=again q=quit\x1b[0m");
                bool decided = false;
                while (!decided) {
                    char ch;
                    if (rx_fifo_try_get(&ch)) {
                        if (ch == 'q' || ch == 'Q') { quit = true; decided = true; }
                        if (ch == 'r' || ch == 'R') { decided = true; }
                    }
                    busy_wait_ms(10);
                }
                game_active = false;
                continue;
            }

            status = "Your turn: aim and fire!";
        }
    }

    game_screen_exit();
}
