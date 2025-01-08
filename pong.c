#include <stdio.h>
#include <stdint.h>
#include <naomi/video.h>
#include <naomi/timer.h>
#include <naomi/maple.h>  // For JVS buttons
#include <string.h>
#define BUTTON_UP (1 << 0)
#define BUTTON_DOWN (1 << 1)

typedef struct {
    float x, y, width, height, speed;
} paddle_t;

typedef struct {
    float x, y, size, speedX, speedY;
} ball_t;

unsigned int player1_score = 0;
unsigned int player2_score = 0;

// Function prototypes
void draw_paddle(paddle_t *paddle, color_t color);
void draw_ball(ball_t *ball);
void update_ball(ball_t *ball, paddle_t *player1, paddle_t *player2);
void update_paddle(paddle_t *paddle, int up, int down);
void draw_pixel(int x, int y, color_t color);  // Pixel drawing function
void draw_scoreboard(void);  // For displaying scores

// Function to set a pixel on the screen at (x, y) with a specific color
void draw_pixel(int x, int y, color_t color) {
    if (x >= 0 && x < video_width() && y >= 0 && y < video_height()) {
        video_draw_pixel(x, y, color);  // Use video_draw_pixel
    }
}

void main()
{
    // Initialize video with 1555 color format
    video_init(VIDEO_COLOR_1555);

    // Set background color (similar to setting it in the example)
    video_set_background_color(rgb(48, 48, 48));

    // Initialize paddles and ball with appropriate sizes for 640x480
    paddle_t player1 = {20, 200, 8, 32, 3};  // Paddle 1 (left), thickness same as ball
    paddle_t player2 = {640 - 30, 200, 8, 32, 3};  // Paddle 2 (right), thickness same as ball
    ball_t game_ball = {320, 240, 8, 2, 2};  // Smaller ball

    unsigned int counter = 0;
    double fps_value = 0.0;

    while (1)
    {
        // Start the FPS and draw time profiling
        int fps = profile_start();
        int draw_time = profile_start();

        // Poll buttons to get the current state for both players
        if (maple_poll_buttons() == 0) {  // Poll buttons
            jvs_buttons_t buttons = maple_buttons_held();  // Get held buttons

            // Poll player 1 controls
            int player1_up = (buttons.player1.up) ? 1 : 0;
            int player1_down = (buttons.player1.down) ? 1 : 0;
            update_paddle(&player1, player1_up, player1_down);

            // Poll player 2 controls
            int player2_up = (buttons.player2.up) ? 1 : 0;
            int player2_down = (buttons.player2.down) ? 1 : 0;
            update_paddle(&player2, player2_up, player2_down);
        }

        // Update the ball's position and handle collisions
        update_ball(&game_ball, &player1, &player2);

        // Clear screen before drawing (similar to video_fill_screen in the example)
        video_fill_screen(rgb(48, 48, 48));  // Set background color (gray)

        // Draw scoreboard at the top
        draw_scoreboard();

        // Draw paddles and ball using individual pixels
        draw_paddle(&player1, rgb(255, 0, 0));  // Player 1 (Red)
        draw_paddle(&player2, rgb(0, 0, 255));  // Player 2 (Blue)
        draw_ball(&game_ball);

        // Debug text (as seen in the example)
        video_draw_debug_text(20, 180, rgb(255, 255, 255), "Pong Game!");
        video_draw_debug_text(20, 200, rgb(200, 200, 20), "Aliveness counter: %d", counter++);
        video_draw_debug_text(20, 220, rgb(200, 200, 20), "Draw Time in uS: %d", profile_end(draw_time));
        video_draw_debug_text(20, 240, rgb(200, 200, 20), "FPS: %.01f, %dx%d", fps_value, video_width(), video_height());

        // Sync the frame to VBlank (as in the example)
        video_display_on_vblank();

        // Calculate FPS based on profiling
        uint32_t uspf = profile_end(fps);
        fps_value = 1000000.0 / (double)uspf;
    }
}

// Function to draw a paddle (vertical line of pixels, red for Player 1, blue for Player 2)
void draw_paddle(paddle_t *paddle, color_t color)
{
    for (int i = 0; i < paddle->height; i++) {
        for (int j = 0; j < paddle->width; j++) {
            draw_pixel(paddle->x + j, paddle->y + i, color);  // Draw paddle pixels
        }
    }
}

// Function to draw the ball (square of pixels)
void draw_ball(ball_t *ball)
{
    for (int i = 0; i < ball->size; i++) {
        for (int j = 0; j < ball->size; j++) {
            draw_pixel(ball->x + i, ball->y + j, rgb(255, 255, 255));  // White ball
        }
    }
}

// Function to update the ball's position and handle collisions
void update_ball(ball_t *ball, paddle_t *player1, paddle_t *player2)
{
    // Move ball based on speed
    ball->x += ball->speedX;
    ball->y += ball->speedY;

    // Ball collision with top and bottom
    if (ball->y <= 0 || ball->y >= 480 - ball->size) {
        ball->speedY = -ball->speedY;  // Reverse vertical direction
    }

    // Ball collision with paddles
    if (ball->x <= player1->x + player1->width && ball->y + ball->size >= player1->y && ball->y <= player1->y + player1->height) {
        ball->speedX = -ball->speedX;  // Reverse horizontal direction
    }
    if (ball->x + ball->size >= player2->x && ball->y + ball->size >= player2->y && ball->y <= player2->y + player2->height) {
        ball->speedX = -ball->speedX;  // Reverse horizontal direction
    }

    // Ball out of bounds (reset position)
    if (ball->x <= 0) {
        player2_score++;  // Player 2 scores
        ball->x = 320;  // Reset ball position
        ball->y = 240;
        ball->speedX = 2;  // Reset speed
        ball->speedY = 2;
    }
    if (ball->x >= 640 - ball->size) {
        player1_score++;  // Player 1 scores
        ball->x = 320;  // Reset ball position
        ball->y = 240;
        ball->speedX = -2;  // Reset speed
        ball->speedY = 2;
    }
}

// Function to update paddles based on button presses
void update_paddle(paddle_t *paddle, int up, int down)
{
    if (up) {
        paddle->y -= paddle->speed;  // Move paddle up
    }
    if (down) {
        paddle->y += paddle->speed;  // Move paddle down
    }

    // Ensure paddles stay within bounds
    if (paddle->y < 0) paddle->y = 0;
    if (paddle->y + paddle->height > 480) paddle->y = 480 - paddle->height;
}

// Function to display the scoreboard
void draw_scoreboard()
{
    char score_text[64];
    sprintf(score_text, "Player 1: %d  Player 2: %d", player1_score, player2_score);
    video_draw_debug_text(320 - (strlen(score_text) * 4), 20, rgb(255, 255, 255), score_text);
}
