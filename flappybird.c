#include <naomi/video.h>
#include <naomi/timer.h>
#include <naomi/maple.h>
#include <string.h>
#include <stdlib.h>
#include "video_prims.h"
#include <stdio.h>
#define SCREEN_W 640
#define SCREEN_H 480

// Bird
typedef struct {
    float x, y, dy;
} bird_t;

// Pipe pair
typedef struct {
    int x;       // horizontal position
    int gap_y;   // top of gap
} pipe_t;

#define MAX_PIPES 5
#define GAP_HEIGHT 120
#define PIPE_WIDTH 40
#define PIPE_SPACING 180

static bird_t bird;
static pipe_t pipes[MAX_PIPES];
static int frame_count = 0;
static int score = 0;
static int game_over = 0;

// Initialize bird and pipes
static void init_game(void) {
    bird.x = SCREEN_W/4;
    bird.y = SCREEN_H/2;
    bird.dy = 0;
    for (int i = 0; i < MAX_PIPES; i++) {
        pipes[i].x = SCREEN_W + i * PIPE_SPACING;
        pipes[i].gap_y = (SCREEN_H - GAP_HEIGHT) / 2;
    }
    frame_count = 0;
    score = 0;
    game_over = 0;
}

static void update_bird(int flap) {
    if (flap) bird.dy = -5.0f;
    bird.dy += 0.3f;           // gravity
    bird.y += bird.dy;
    if (bird.y < 0)    bird.y = 0,    bird.dy = 0;
    if (bird.y > SCREEN_H-16) bird.y = SCREEN_H-16, game_over = 1;
}

static void update_pipes(void) {
    for (int i = 0; i < MAX_PIPES; i++) {
        pipes[i].x -= 2;
        // recycle pipe off-screen
        if (pipes[i].x + PIPE_WIDTH < 0) {
            pipes[i].x = SCREEN_W;
            pipes[i].gap_y = rand() % (SCREEN_H - GAP_HEIGHT);
        }
        // score when bird passes pipe
        if (!game_over && pipes[i].x + PIPE_WIDTH/2 == (int)bird.x) {
            score++;
        }
    }
}

static int check_collision(void) {
    // Bird rectangle: 16x16
    int bx0 = bird.x, by0 = bird.y, bx1 = bx0 + 16, by1 = by0 + 16;
    for (int i = 0; i < MAX_PIPES; i++) {
        int px = pipes[i].x;
        // top pipe
        if (bx1 > px && bx0 < px + PIPE_WIDTH) {
            if (by0 < pipes[i].gap_y || by1 > pipes[i].gap_y + GAP_HEIGHT) {
                return 1;
            }
        }
    }
    return 0;
}

int main(void) {
    vp_init(VIDEO_COLOR_1555);
    init_game();

    double fps = 0;
    while (1) {
        int t0 = profile_start();

        // Input
        int flap = 0;
        if (maple_poll_buttons() == 0) {
            jvs_buttons_t btn = maple_buttons_held();
            flap = btn.player1.up;
        }

        // Update
        if (!game_over) {
            update_bird(flap);
            if (frame_count % 3 == 0) update_pipes();
            if (check_collision()) game_over = 1;
        } else if (flap) {
            init_game();
        }
        frame_count++;

        // Draw
        vp_clear(rgb(0,0,0));

        // Pipes
        for (int i = 0; i < MAX_PIPES; i++) {
            int x = pipes[i].x;
            // top
            vp_rect(x, 0, PIPE_WIDTH, pipes[i].gap_y, rgb(0,255,0));
            // bottom
            vp_rect(x, pipes[i].gap_y+GAP_HEIGHT,
                    PIPE_WIDTH, SCREEN_H - (pipes[i].gap_y+GAP_HEIGHT),
                    rgb(0,255,0));
        }

        // Bird
        vp_rect(bird.x, bird.y, 16, 16, game_over ? rgb(255,0,0) : rgb(255,255,0));

        // Score & status
        char buf[32];
        sprintf(buf, "Score: %d", score);
        video_draw_debug_text(20, 20, rgb(255,255,255), buf);
        if (game_over) {
            video_draw_debug_text( SCREEN_W/2 - 32, SCREEN_H/2,
                                   rgb(255,255,255),
                                   "GAME OVER");
            video_draw_debug_text( SCREEN_W/2 - 64, SCREEN_H/2+16,
                                   rgb(255,255,255),
                                   "Press Up to restart");
        }

        // Debug FPS
        video_draw_debug_text(20, SCREEN_H-20, rgb(200,200,20),
                              "FPS: %.1f", fps);

        vp_vblank();
        fps = 1000000.0 / profile_end(t0);
    }
    return 0;
}
