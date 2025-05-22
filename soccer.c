#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <naomi/video.h>
#include <naomi/ta.h>
#include <naomi/maple.h>
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define PLAYER_SIZE 18
#define BALL_SIZE 10
#define PLAYER_SPEED 2.0f
#define BALL_FRICTION 0.98f
#define KICK_STRENGTH 4.0f
#define GOAL_WIDTH 8
#define GOAL_HEIGHT 120
#define GOAL_Y ((SCREEN_HEIGHT-GOAL_HEIGHT)/2)
#define MAX_SCORE 5

typedef struct {
    float x, y;
    float vx, vy;
    int score;
    color_t color;
} player_t;

typedef struct {
    float x, y;
    float vx, vy;
} ball_t;

// Drawing helpers
void draw_filled_rect_ta(float x, float y, float width, float height, color_t color) {
    vertex_t verts[4];
    verts[0].x = x;           verts[0].y = y + height; verts[0].z = 1.0f;
    verts[1].x = x;           verts[1].y = y;          verts[1].z = 1.0f;
    verts[2].x = x + width;   verts[2].y = y;          verts[2].z = 1.0f;
    verts[3].x = x + width;   verts[3].y = y + height; verts[3].z = 1.0f;
    ta_fill_box(TA_CMD_POLYGON_TYPE_OPAQUE, verts, color);
}

void draw_circle_outline_ta(float cx, float cy, float radius, float thickness, color_t color) {
    int segments = 24;
    float angle_step = 2.0f * M_PI / segments;
    for (int i = 0; i < segments; i++) {
        float a1 = i * angle_step;
        float a2 = (i + 1) * angle_step;
        float x1 = cx + cosf(a1) * radius;
        float y1 = cy + sinf(a1) * radius;
        float x2 = cx + cosf(a2) * radius;
        float y2 = cy + sinf(a2) * radius;
        // Draw as a thin rectangle (line)
        vertex_t verts[4];
        float dx = x2 - x1, dy = y2 - y1, len = sqrtf(dx*dx + dy*dy);
        float nx = -dy / len * thickness * 0.5f;
        float ny = dx / len * thickness * 0.5f;
        verts[0].x = x1 + nx; verts[0].y = y1 + ny; verts[0].z = 1.0f;
        verts[1].x = x1 - nx; verts[1].y = y1 - ny; verts[1].z = 1.0f;
        verts[2].x = x2 - nx; verts[2].y = y2 - ny; verts[2].z = 1.0f;
        verts[3].x = x2 + nx; verts[3].y = y2 + ny; verts[3].z = 1.0f;
        ta_fill_box(TA_CMD_POLYGON_TYPE_OPAQUE, verts, color);
    }
}

// Game helpers
void reset_positions(player_t *p1, player_t *p2, ball_t *ball) {
    p1->x = 40; p1->y = SCREEN_HEIGHT/2 - PLAYER_SIZE/2; p1->vx = 0; p1->vy = 0;
    p2->x = SCREEN_WIDTH-40-PLAYER_SIZE; p2->y = SCREEN_HEIGHT/2 - PLAYER_SIZE/2; p2->vx = 0; p2->vy = 0;
    ball->x = SCREEN_WIDTH/2 - BALL_SIZE/2; ball->y = SCREEN_HEIGHT/2 - BALL_SIZE/2;
    ball->vx = ((rand() % 2) ? 2.0f : -2.0f); ball->vy = ((rand() % 3) - 1) * 1.0f;
}

void draw_player(player_t *p) {
    // Body
    draw_filled_rect_ta(p->x, p->y, PLAYER_SIZE, PLAYER_SIZE, p->color);
    // Head
    draw_filled_rect_ta(p->x + 4, p->y - 6, 10, 6, rgb(255, 220, 177));
    // Feet
    draw_filled_rect_ta(p->x + 2, p->y + PLAYER_SIZE, 4, 4, rgb(0,0,0));
    draw_filled_rect_ta(p->x + PLAYER_SIZE - 6, p->y + PLAYER_SIZE, 4, 4, rgb(0,0,0));
}

void draw_ball(ball_t *b) {
    draw_filled_rect_ta(b->x, b->y, BALL_SIZE, BALL_SIZE, rgb(255,255,255));
    draw_filled_rect_ta(b->x+3, b->y+3, 4, 4, rgb(0,0,0)); // "8-bit" spot
}

void draw_goals(void) {
    // Left goal
    draw_filled_rect_ta(0, GOAL_Y, GOAL_WIDTH, GOAL_HEIGHT, rgb(255,255,0));
    // Right goal
    draw_filled_rect_ta(SCREEN_WIDTH-GOAL_WIDTH, GOAL_Y, GOAL_WIDTH, GOAL_HEIGHT, rgb(255,255,0));
}

void draw_field(void) {
    ta_set_background_color(rgb(34,139,34));
    draw_goals();
    // Center line
    draw_filled_rect_ta(SCREEN_WIDTH/2-2, 0, 4, SCREEN_HEIGHT, rgb(255,255,255));
    // Center circle
    for(int r=40; r<44; r++)
        draw_circle_outline_ta(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, r, 1.5f, rgb(255,255,255));
}

void draw_scoreboard(player_t *p1, player_t *p2) {
    draw_filled_rect_ta(220, 20, 200, 40, rgb(0, 50, 0));
    char buf[32];
    sprintf(buf, "RED: %d", p1->score);
    video_draw_debug_text(240, 30, rgb(255,0,0), buf);
    sprintf(buf, "BLU: %d", p2->score);
    video_draw_debug_text(360, 30, rgb(0,100,255), buf);
}

void update_player(player_t *p, int up, int down, int left, int right) {
    if(up) p->y -= PLAYER_SPEED;
    if(down) p->y += PLAYER_SPEED;
    if(left) p->x -= PLAYER_SPEED;
    if(right) p->x += PLAYER_SPEED;
    // Clamp to field
    if(p->x < 0) p->x = 0;
    if(p->x > SCREEN_WIDTH-PLAYER_SIZE) p->x = SCREEN_WIDTH-PLAYER_SIZE;
    if(p->y < 0) p->y = 0;
    if(p->y > SCREEN_HEIGHT-PLAYER_SIZE) p->y = SCREEN_HEIGHT-PLAYER_SIZE;
}

void update_ball(ball_t *ball) {
    ball->x += ball->vx;
    ball->y += ball->vy;
    ball->vx *= BALL_FRICTION;
    ball->vy *= BALL_FRICTION;
    // Bounce off top/bottom
    if(ball->y < 0) { ball->y = 0; ball->vy = -ball->vy; }
    if(ball->y > SCREEN_HEIGHT-BALL_SIZE) { ball->y = SCREEN_HEIGHT-BALL_SIZE; ball->vy = -ball->vy; }
    // Bounce off left/right (except in goal)
    if(ball->x < 0) { ball->x = 0; ball->vx = -ball->vx; }
    if(ball->x > SCREEN_WIDTH-BALL_SIZE) { ball->x = SCREEN_WIDTH-BALL_SIZE; ball->vx = -ball->vx; }
}

// Returns 1 if goal for p2, 2 if goal for p1, 0 otherwise
int check_goal(ball_t *ball) {
    // Left goal
    if(ball->x < GOAL_WIDTH &&
       ball->y + BALL_SIZE > GOAL_Y &&
       ball->y < GOAL_Y + GOAL_HEIGHT) {
        return 2;
    }
    // Right goal
    if(ball->x + BALL_SIZE > SCREEN_WIDTH-GOAL_WIDTH &&
       ball->y + BALL_SIZE > GOAL_Y &&
       ball->y < GOAL_Y + GOAL_HEIGHT) {
        return 1;
    }
    return 0;
}

// Try to kick the ball if close and kick button pressed
void try_kick(player_t *p, ball_t *ball, int kick) {
    if(!kick) return;
    float px = p->x + PLAYER_SIZE/2;
    float py = p->y + PLAYER_SIZE/2;
    float bx = ball->x + BALL_SIZE/2;
    float by = ball->y + BALL_SIZE/2;
    float dx = bx - px;
    float dy = by - py;
    float dist = sqrtf(dx*dx + dy*dy);
    if(dist < PLAYER_SIZE) {
        // Kick in direction away from player
        float norm = sqrtf(dx*dx + dy*dy);
        if(norm < 1.0f) norm = 1.0f;
        ball->vx += (dx / norm) * KICK_STRENGTH;
        ball->vy += (dy / norm) * KICK_STRENGTH;
    }
}

int main(void) {
    video_init(VIDEO_COLOR_1555);
    ta_set_background_color(rgb(34,139,34));
    srand(12345);

    player_t p1 = {0}, p2 = {0};
    ball_t ball = {0};
    p1.color = rgb(255,0,0);
    p2.color = rgb(0,100,255);
    p1.score = 0; p2.score = 0;
    reset_positions(&p1, &p2, &ball);

    int goal_pause = 0;
    int winner = 0;

    while(1) {
        // Poll controls
        maple_poll_buttons();
        jvs_buttons_t buttons = maple_buttons_held();

        // Player 1: up/down/left/right/start = kick
        update_player(&p1, buttons.player1.up, buttons.player1.down, buttons.player1.left, buttons.player1.right);
        // Player 2: up/down/left/right/start = kick
        update_player(&p2, buttons.player2.up, buttons.player2.down, buttons.player2.left, buttons.player2.right);

        // Kicking
        try_kick(&p1, &ball, buttons.player1.button1);
        try_kick(&p2, &ball, buttons.player2.button1);

        // Only move ball if not in goal pause
        if(goal_pause == 0 && winner == 0) {
            update_ball(&ball);
            int goal = check_goal(&ball);
            if(goal == 1) { p1.score++; goal_pause = 90; reset_positions(&p1, &p2, &ball); }
            if(goal == 2) { p2.score++; goal_pause = 90; reset_positions(&p1, &p2, &ball); }
            if(p1.score >= MAX_SCORE) { winner = 1; goal_pause = 180; }
            if(p2.score >= MAX_SCORE) { winner = 2; goal_pause = 180; }
        } else if(goal_pause > 0) {
            goal_pause--;
            if(goal_pause == 0 && winner) {
                p1.score = 0; p2.score = 0; winner = 0;
                reset_positions(&p1, &p2, &ball);
            }
        }

        // Render
        ta_commit_begin();
        draw_field();
        draw_ball(&ball);
        draw_player(&p1);
        draw_player(&p2);
        draw_scoreboard(&p1, &p2);

        if(winner) {
            char buf[32];
            sprintf(buf, "PLAYER %d WINS!", winner);
            video_draw_debug_text(SCREEN_WIDTH/2-80, SCREEN_HEIGHT/2-10, rgb(255,255,0), buf);
            video_draw_debug_text(SCREEN_WIDTH/2-100, SCREEN_HEIGHT/2+10, rgb(255,255,255), "Press START to restart");
        }
        ta_commit_end();
        ta_render();

        video_display_on_vblank();
    }

    return 0;
}
