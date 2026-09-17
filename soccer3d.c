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

#define FIELD_WIDTH 14.0f
#define FIELD_LENGTH 22.0f
#define HALF_W (FIELD_WIDTH * 0.5f)
#define HALF_L (FIELD_LENGTH * 0.5f)

#define GOAL_WIDTH 5.0f
#define GOAL_HEIGHT 2.2f
#define GOAL_DEPTH 1.35f

#define PLAYER_RADIUS 0.48f
#define PLAYER_SPEED 0.115f
#define PLAYER_SPRINT 1.28f
#define KICK_RANGE 1.15f
#define KICK_SPEED 0.39f
#define KICK_LIFT 0.155f
#define KICK_COOLDOWN 11

#define BALL_RADIUS 0.28f
#define BALL_FRICTION 0.988f
#define BALL_AIR_FRICTION 0.997f
#define BALL_GRAVITY 0.0105f
#define BALL_BOUNCE 0.44f
#define WALL_BOUNCE 0.76f

#define MAX_SCORE 5
#define GOAL_PAUSE_FRAMES 95
#define WIN_PAUSE_FRAMES 210

#define CAMERA_FOCAL 445.0f
#define CAMERA_NEAR 0.55f

typedef struct {
    float x, y, z;
} vec3_t;

typedef struct {
    float x, z;
    float vx, vz;
    float face_x, face_z;
    float anim;
    int kick_cd;
    int score;
    color_t shirt;
    color_t shirt_side;
} player_t;

typedef struct {
    float x, y, z;
    float vx, vy, vz;
} ball_t;

typedef struct {
    vec3_t eye;
    vec3_t right;
    vec3_t up;
    vec3_t forward;
} camera_t;

static camera_t g_camera;
static vec3_t g_circle[25];

static inline float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float dot3(vec3_t a, vec3_t b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline vec3_t sub3(vec3_t a, vec3_t b)
{
    vec3_t r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

static inline vec3_t cross3(vec3_t a, vec3_t b)
{
    vec3_t r = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return r;
}

static vec3_t normalize3(vec3_t v)
{
    float d2 = dot3(v, v);
    if (d2 < 0.000001f) {
        vec3_t z = {0.0f, 0.0f, 0.0f};
        return z;
    }
    float inv = 1.0f / sqrtf(d2);
    v.x *= inv;
    v.y *= inv;
    v.z *= inv;
    return v;
}

static void camera_init(void)
{
    /* Fixed broadcast/arcade camera. Nothing here changes per frame. */
    g_camera.eye = (vec3_t){ 0.0f, 9.0f, -16.0f };
    vec3_t target = { 0.0f, 0.35f, 2.4f };
    vec3_t world_up = { 0.0f, 1.0f, 0.0f };

    g_camera.forward = normalize3(sub3(target, g_camera.eye));
    g_camera.right = normalize3(cross3(world_up, g_camera.forward));
    g_camera.up = normalize3(cross3(g_camera.forward, g_camera.right));
}

static int project_point(vec3_t p, vertex_t *out)
{
    vec3_t rel = sub3(p, g_camera.eye);
    float cx = dot3(rel, g_camera.right);
    float cy = dot3(rel, g_camera.up);
    float cz = dot3(rel, g_camera.forward);

    if (cz <= CAMERA_NEAR) return 0;

    float invz = 1.0f / cz;
    out->x = (SCREEN_WIDTH * 0.5f) + (cx * CAMERA_FOCAL * invz);
    out->y = (SCREEN_HEIGHT * 0.5f) - (cy * CAMERA_FOCAL * invz);

    /* PVR depth is happiest with larger values nearer the camera. */
    out->z = invz;
    return 1;
}

static void draw_quad_world(vec3_t a, vec3_t b, vec3_t c, vec3_t d, color_t color)
{
    vertex_t q[4];
    if (!project_point(a, &q[0])) return;
    if (!project_point(b, &q[1])) return;
    if (!project_point(c, &q[2])) return;
    if (!project_point(d, &q[3])) return;
    ta_fill_box(TA_CMD_POLYGON_TYPE_OPAQUE, q, color);
}

static void draw_ground_rect(float x0, float z0, float x1, float z1, float y, color_t color)
{
    draw_quad_world(
        (vec3_t){x0, y, z1},
        (vec3_t){x0, y, z0},
        (vec3_t){x1, y, z0},
        (vec3_t){x1, y, z1},
        color
    );
}

static void draw_ground_line(float x0, float z0, float x1, float z1, float thickness, color_t color)
{
    float dx = x1 - x0;
    float dz = z1 - z0;
    float len2 = dx * dx + dz * dz;
    if (len2 < 0.00001f) return;

    float inv = 1.0f / sqrtf(len2);
    float px = -dz * inv * thickness * 0.5f;
    float pz = dx * inv * thickness * 0.5f;
    const float y = 0.018f;

    draw_quad_world(
        (vec3_t){x0 + px, y, z0 + pz},
        (vec3_t){x0 - px, y, z0 - pz},
        (vec3_t){x1 - px, y, z1 - pz},
        (vec3_t){x1 + px, y, z1 + pz},
        color
    );
}

static void draw_cuboid(float x0, float y0, float z0,
                        float x1, float y1, float z1,
                        color_t top, color_t side, color_t dark)
{
    vec3_t v[8] = {
        {x0, y0, z0}, {x1, y0, z0}, {x0, y1, z0}, {x1, y1, z0},
        {x0, y0, z1}, {x1, y0, z1}, {x0, y1, z1}, {x1, y1, z1}
    };

    /* Top, then four walls, then bottom. Z buffer sorts the visible pieces. */
    draw_quad_world(v[6], v[2], v[3], v[7], top);
    draw_quad_world(v[2], v[0], v[1], v[3], side);
    draw_quad_world(v[7], v[3], v[1], v[5], side);
    draw_quad_world(v[4], v[6], v[7], v[5], dark);
    draw_quad_world(v[6], v[4], v[0], v[2], dark);
    draw_quad_world(v[0], v[4], v[5], v[1], dark);
}

static void build_static_geometry(void)
{
    for (int i = 0; i <= 24; i++) {
        float a = ((float)i / 24.0f) * (2.0f * (float)M_PI);
        g_circle[i].x = cosf(a) * 2.0f;
        g_circle[i].y = 0.018f;
        g_circle[i].z = sinf(a) * 2.0f;
    }
}

static void draw_pitch(void)
{
    /* Mowed strips. Eleven big quads are substantially cheaper than a textured field. */
    const float stripe = FIELD_LENGTH / 11.0f;
    for (int i = 0; i < 11; i++) {
        float z0 = -HALF_L + (stripe * i);
        float z1 = z0 + stripe;
        color_t grass = (i & 1) ? rgb(28, 123, 49) : rgb(34, 139, 55);
        draw_ground_rect(-HALF_W, z0, HALF_W, z1, 0.0f, grass);
    }

    color_t white = rgb(235, 242, 235);

    /* Boundary. */
    draw_ground_line(-HALF_W, -HALF_L,  HALF_W, -HALF_L, 0.10f, white);
    draw_ground_line(-HALF_W,  HALF_L,  HALF_W,  HALF_L, 0.10f, white);
    draw_ground_line(-HALF_W, -HALF_L, -HALF_W,  HALF_L, 0.10f, white);
    draw_ground_line( HALF_W, -HALF_L,  HALF_W,  HALF_L, 0.10f, white);

    /* Halfway line and circle. */
    draw_ground_line(-HALF_W, 0.0f, HALF_W, 0.0f, 0.08f, white);
    for (int i = 0; i < 24; i++) {
        draw_ground_line(g_circle[i].x, g_circle[i].z,
                         g_circle[i + 1].x, g_circle[i + 1].z,
                         0.075f, white);
    }
    draw_ground_rect(-0.09f, -0.09f, 0.09f, 0.09f, 0.020f, white);

    /* Penalty boxes. */
    const float box_w = 7.2f;
    const float box_d = 3.2f;
    draw_ground_line(-box_w * 0.5f, -HALF_L, -box_w * 0.5f, -HALF_L + box_d, 0.075f, white);
    draw_ground_line( box_w * 0.5f, -HALF_L,  box_w * 0.5f, -HALF_L + box_d, 0.075f, white);
    draw_ground_line(-box_w * 0.5f, -HALF_L + box_d, box_w * 0.5f, -HALF_L + box_d, 0.075f, white);

    draw_ground_line(-box_w * 0.5f, HALF_L, -box_w * 0.5f, HALF_L - box_d, 0.075f, white);
    draw_ground_line( box_w * 0.5f, HALF_L,  box_w * 0.5f, HALF_L - box_d, 0.075f, white);
    draw_ground_line(-box_w * 0.5f, HALF_L - box_d, box_w * 0.5f, HALF_L - box_d, 0.075f, white);
}

static void draw_goal(float z, float dir)
{
    const float post = 0.12f;
    const float x0 = GOAL_WIDTH * 0.5f;
    const float back_z = z + dir * GOAL_DEPTH;
    color_t white = rgb(238, 238, 230);
    color_t shade = rgb(190, 194, 185);
    color_t dark = rgb(145, 150, 145);

    /* Front posts and bar. */
    draw_cuboid(-x0 - post, 0.0f, z - post,
                -x0 + post, GOAL_HEIGHT, z + post,
                white, shade, dark);
    draw_cuboid(x0 - post, 0.0f, z - post,
                x0 + post, GOAL_HEIGHT, z + post,
                white, shade, dark);
    draw_cuboid(-x0 - post, GOAL_HEIGHT - post, z - post,
                x0 + post, GOAL_HEIGHT + post, z + post,
                white, shade, dark);

    /* Rear frame makes the goal unmistakably three-dimensional. */
    float za = (z < back_z) ? z : back_z;
    float zb = (z < back_z) ? back_z : z;
    draw_cuboid(-x0 - post, 0.0f, za, -x0 + post, post * 2.0f, zb, shade, dark, dark);
    draw_cuboid( x0 - post, 0.0f, za,  x0 + post, post * 2.0f, zb, shade, dark, dark);
    draw_cuboid(-x0 - post, GOAL_HEIGHT - post, back_z - post,
                x0 + post, GOAL_HEIGHT + post, back_z + post,
                shade, dark, dark);
}

static void draw_stadium(void)
{
    /* Cheap perimeter boards. These add depth cues without textures or extra assets. */
    color_t board_top = rgb(48, 58, 70);
    color_t board_side = rgb(31, 37, 46);
    color_t board_dark = rgb(22, 27, 34);

    draw_cuboid(-HALF_W - 0.65f, 0.0f, -HALF_L - 1.0f,
                -HALF_W - 0.35f, 0.72f, HALF_L + 1.0f,
                board_top, board_side, board_dark);
    draw_cuboid(HALF_W + 0.35f, 0.0f, -HALF_L - 1.0f,
                HALF_W + 0.65f, 0.72f, HALF_L + 1.0f,
                board_top, board_side, board_dark);
    draw_cuboid(-HALF_W - 0.65f, 0.0f, HALF_L + 1.0f,
                HALF_W + 0.65f, 0.72f, HALF_L + 1.3f,
                board_top, board_side, board_dark);
}

static void draw_player(const player_t *p, int player_num)
{
    float bob = sinf(p->anim) * 0.035f;
    float x = p->x;
    float z = p->z;

    color_t skin = rgb(234, 190, 145);
    color_t skin_side = rgb(205, 158, 118);
    color_t shorts = (player_num == 1) ? rgb(105, 12, 18) : rgb(18, 45, 110);
    color_t shorts_dark = (player_num == 1) ? rgb(68, 8, 12) : rgb(10, 27, 67);
    color_t boot = rgb(28, 28, 31);

    /* Ground shadow. */
    draw_ground_rect(x - 0.38f, z - 0.22f, x + 0.38f, z + 0.22f, 0.012f, rgb(20, 82, 35));

    /* Legs. */
    draw_cuboid(x - 0.30f, 0.05f + bob, z - 0.20f,
                x - 0.05f, 0.78f + bob, z + 0.20f,
                skin, skin_side, skin_side);
    draw_cuboid(x + 0.05f, 0.05f + bob, z - 0.20f,
                x + 0.30f, 0.78f + bob, z + 0.20f,
                skin, skin_side, skin_side);

    /* Boots. */
    draw_cuboid(x - 0.34f, 0.01f, z - 0.30f,
                x - 0.02f, 0.18f, z + 0.20f,
                boot, boot, rgb(10, 10, 12));
    draw_cuboid(x + 0.02f, 0.01f, z - 0.30f,
                x + 0.34f, 0.18f, z + 0.20f,
                boot, boot, rgb(10, 10, 12));

    /* Shorts and torso. */
    draw_cuboid(x - 0.38f, 0.72f + bob, z - 0.25f,
                x + 0.38f, 1.05f + bob, z + 0.25f,
                shorts, shorts_dark, shorts_dark);
    draw_cuboid(x - 0.42f, 1.00f + bob, z - 0.28f,
                x + 0.42f, 1.75f + bob, z + 0.28f,
                p->shirt, p->shirt_side, p->shirt_side);

    /* Head. */
    draw_cuboid(x - 0.27f, 1.76f + bob, z - 0.24f,
                x + 0.27f, 2.30f + bob, z + 0.24f,
                skin, skin_side, rgb(176, 126, 90));
}

static void draw_ball(const ball_t *b)
{
    float r = BALL_RADIUS;

    /* Shadow gets smaller/lighter in a real game; a compact dark quad is cheaper. */
    draw_ground_rect(b->x - r * 0.72f, b->z - r * 0.48f,
                     b->x + r * 0.72f, b->z + r * 0.48f,
                     0.014f, rgb(18, 72, 30));

    draw_cuboid(b->x - r, b->y - r, b->z - r,
                b->x + r, b->y + r, b->z + r,
                rgb(248, 248, 242), rgb(214, 214, 208), rgb(175, 175, 171));

    /* Tiny black cap gives the low-poly ball a soccer read without a texture. */
    const float p = 0.105f;
    draw_cuboid(b->x - p, b->y + r - 0.025f, b->z - p,
                b->x + p, b->y + r + 0.025f, b->z + p,
                rgb(28, 28, 30), rgb(20, 20, 22), rgb(12, 12, 14));
}

static void reset_positions(player_t *p1, player_t *p2, ball_t *ball, int serve_dir)
{
    p1->x = 0.0f;
    p1->z = -5.8f;
    p1->vx = p1->vz = 0.0f;
    p1->face_x = 0.0f;
    p1->face_z = 1.0f;
    p1->kick_cd = 0;

    p2->x = 0.0f;
    p2->z = 5.8f;
    p2->vx = p2->vz = 0.0f;
    p2->face_x = 0.0f;
    p2->face_z = -1.0f;
    p2->kick_cd = 0;

    ball->x = 0.0f;
    ball->y = BALL_RADIUS + 0.02f;
    ball->z = 0.0f;
    ball->vx = 0.0f;
    ball->vy = 0.0f;
    ball->vz = 0.055f * (float)serve_dir;
}

static void update_player(player_t *p, int up, int down, int left, int right, int sprint)
{
    float dx = (right ? 1.0f : 0.0f) - (left ? 1.0f : 0.0f);
    float dz = (up ? 1.0f : 0.0f) - (down ? 1.0f : 0.0f);

    if (dx != 0.0f && dz != 0.0f) {
        dx *= 0.70710678f;
        dz *= 0.70710678f;
    }

    float speed = PLAYER_SPEED * (sprint ? PLAYER_SPRINT : 1.0f);
    p->vx = dx * speed;
    p->vz = dz * speed;
    p->x += p->vx;
    p->z += p->vz;

    if (dx != 0.0f || dz != 0.0f) {
        p->face_x = dx;
        p->face_z = dz;
        p->anim += sprint ? 0.34f : 0.26f;
    }

    p->x = clampf(p->x, -HALF_W + PLAYER_RADIUS, HALF_W - PLAYER_RADIUS);
    p->z = clampf(p->z, -HALF_L + PLAYER_RADIUS, HALF_L - PLAYER_RADIUS);

    if (p->kick_cd > 0) p->kick_cd--;
}

static void resolve_player_collision(player_t *a, player_t *b)
{
    float dx = b->x - a->x;
    float dz = b->z - a->z;
    float min_d = PLAYER_RADIUS * 2.0f;
    float d2 = dx * dx + dz * dz;

    if (d2 > 0.0001f && d2 < min_d * min_d) {
        float d = sqrtf(d2);
        float nx = dx / d;
        float nz = dz / d;
        float push = (min_d - d) * 0.5f;
        a->x -= nx * push;
        a->z -= nz * push;
        b->x += nx * push;
        b->z += nz * push;
    }
}

static void body_ball_collision(const player_t *p, ball_t *b)
{
    if (b->y > 0.95f) return;

    float dx = b->x - p->x;
    float dz = b->z - p->z;
    float min_d = PLAYER_RADIUS + BALL_RADIUS;
    float d2 = dx * dx + dz * dz;

    if (d2 > 0.0001f && d2 < min_d * min_d) {
        float d = sqrtf(d2);
        float nx = dx / d;
        float nz = dz / d;
        float push = min_d - d;
        b->x += nx * push;
        b->z += nz * push;
        b->vx += nx * 0.025f + p->vx * 0.55f;
        b->vz += nz * 0.025f + p->vz * 0.55f;
    }
}

static void try_kick(player_t *p, ball_t *b, int kick)
{
    if (!kick || p->kick_cd > 0 || b->y > 1.05f) return;

    float dx = b->x - p->x;
    float dz = b->z - p->z;
    float d2 = dx * dx + dz * dz;
    if (d2 > KICK_RANGE * KICK_RANGE) return;

    float bx = p->face_x;
    float bz = p->face_z;

    if (d2 > 0.0001f) {
        float inv = 1.0f / sqrtf(d2);
        /* Mostly facing direction, partly ball-away direction: responsive but controllable. */
        bx = bx * 0.72f + dx * inv * 0.28f;
        bz = bz * 0.72f + dz * inv * 0.28f;
        float n2 = bx * bx + bz * bz;
        if (n2 > 0.0001f) {
            float ninv = 1.0f / sqrtf(n2);
            bx *= ninv;
            bz *= ninv;
        }
    }

    b->vx = bx * KICK_SPEED + p->vx * 0.45f;
    b->vz = bz * KICK_SPEED + p->vz * 0.45f;
    b->vy = KICK_LIFT;
    p->kick_cd = KICK_COOLDOWN;
}

static void update_ball(ball_t *b)
{
    b->x += b->vx;
    b->y += b->vy;
    b->z += b->vz;

    if (b->y > BALL_RADIUS + 0.025f || b->vy > 0.0f) {
        b->vx *= BALL_AIR_FRICTION;
        b->vz *= BALL_AIR_FRICTION;
        b->vy -= BALL_GRAVITY;
    } else {
        b->vx *= BALL_FRICTION;
        b->vz *= BALL_FRICTION;
    }

    if (b->y < BALL_RADIUS) {
        b->y = BALL_RADIUS;
        if (b->vy < -0.025f) {
            b->vy = -b->vy * BALL_BOUNCE;
        } else {
            b->vy = 0.0f;
        }
    }

    if (b->x < -HALF_W + BALL_RADIUS) {
        b->x = -HALF_W + BALL_RADIUS;
        b->vx = fabsf(b->vx) * WALL_BOUNCE;
    } else if (b->x > HALF_W - BALL_RADIUS) {
        b->x = HALF_W - BALL_RADIUS;
        b->vx = -fabsf(b->vx) * WALL_BOUNCE;
    }

    /* Endline behaves as a wall outside the mouth. Inside it, the goal volume is open. */
    int in_mouth = (fabsf(b->x) < (GOAL_WIDTH * 0.5f - BALL_RADIUS * 0.25f)) &&
                   (b->y < GOAL_HEIGHT - BALL_RADIUS * 0.25f);

    if (!in_mouth) {
        if (b->z < -HALF_L + BALL_RADIUS) {
            b->z = -HALF_L + BALL_RADIUS;
            b->vz = fabsf(b->vz) * WALL_BOUNCE;
        } else if (b->z > HALF_L - BALL_RADIUS) {
            b->z = HALF_L - BALL_RADIUS;
            b->vz = -fabsf(b->vz) * WALL_BOUNCE;
        }
    } else {
        /* Rear goal frame collision. */
        if (b->z < -HALF_L - GOAL_DEPTH + BALL_RADIUS) {
            b->z = -HALF_L - GOAL_DEPTH + BALL_RADIUS;
            b->vz = fabsf(b->vz) * 0.5f;
        } else if (b->z > HALF_L + GOAL_DEPTH - BALL_RADIUS) {
            b->z = HALF_L + GOAL_DEPTH - BALL_RADIUS;
            b->vz = -fabsf(b->vz) * 0.5f;
        }
    }

    if (fabsf(b->vx) < 0.00025f) b->vx = 0.0f;
    if (fabsf(b->vz) < 0.00025f) b->vz = 0.0f;
}

/* Returns 1 for player 1 scoring into +Z, 2 for player 2 scoring into -Z. */
static int check_goal(const ball_t *b)
{
    int in_mouth = fabsf(b->x) < (GOAL_WIDTH * 0.5f) &&
                   b->y < GOAL_HEIGHT;
    if (!in_mouth) return 0;

    if (b->z > HALF_L + BALL_RADIUS * 0.18f) return 1;
    if (b->z < -HALF_L - BALL_RADIUS * 0.18f) return 2;
    return 0;
}

static void render_world(const player_t *p1, const player_t *p2, const ball_t *ball)
{
    ta_commit_begin();

    draw_pitch();
    draw_stadium();
    draw_goal(-HALF_L, -1.0f);
    draw_goal( HALF_L,  1.0f);

    /* Z buffer handles overlap; draw order is organized mostly for cache/readability. */
    draw_player(p2, 2);
    draw_player(p1, 1);
    draw_ball(ball);

    ta_commit_end();
    ta_render();
}

static void draw_hud(const player_t *p1, const player_t *p2, int winner, int goal_flash)
{
    video_draw_debug_text(20, 16, rgb(255, 235, 235), "RED %d", p1->score);
    video_draw_debug_text(SCREEN_WIDTH - 92, 16, rgb(220, 230, 255), "BLUE %d", p2->score);
    video_draw_debug_text(SCREEN_WIDTH / 2 - 72, 16, rgb(245, 245, 220), "NAOMI 3D SOCCER");

    if (goal_flash > 0 && !winner) {
        video_draw_debug_text(SCREEN_WIDTH / 2 - 28, 62, rgb(255, 235, 80), "GOAL!");
    }

    if (winner) {
        video_draw_debug_text(SCREEN_WIDTH / 2 - 70, SCREEN_HEIGHT / 2 - 18,
                              rgb(255, 235, 80),
                              winner == 1 ? "RED WINS!" : "BLUE WINS!");
        video_draw_debug_text(SCREEN_WIDTH / 2 - 78, SCREEN_HEIGHT / 2 + 2,
                              rgb(255, 255, 255), "NEW MATCH IN A MOMENT");
    }

    video_draw_debug_text(18, SCREEN_HEIGHT - 22, rgb(220, 220, 220),
                          "B1 KICK   B2 SPRINT");
}

int main(void)
{
    video_init(VIDEO_COLOR_1555);
    ta_set_background_color(rgb(82, 130, 168));
    camera_init();
    build_static_geometry();

    player_t p1 = {0};
    player_t p2 = {0};
    ball_t ball = {0};

    p1.shirt = rgb(226, 35, 42);
    p1.shirt_side = rgb(151, 19, 25);
    p2.shirt = rgb(45, 92, 229);
    p2.shirt_side = rgb(24, 51, 151);

    reset_positions(&p1, &p2, &ball, 1);

    int pause = 0;
    int winner = 0;
    int last_scorer = 0;

    while (1) {
        maple_poll_buttons();
        jvs_buttons_t buttons = maple_buttons_held();

        if (winner == 0 && pause == 0) {
            update_player(&p1,
                          buttons.player1.up, buttons.player1.down,
                          buttons.player1.left, buttons.player1.right,
                          buttons.player1.button2);
            update_player(&p2,
                          buttons.player2.up, buttons.player2.down,
                          buttons.player2.left, buttons.player2.right,
                          buttons.player2.button2);

            resolve_player_collision(&p1, &p2);
            body_ball_collision(&p1, &ball);
            body_ball_collision(&p2, &ball);
            try_kick(&p1, &ball, buttons.player1.button1);
            try_kick(&p2, &ball, buttons.player2.button1);
            update_ball(&ball);

            int goal = check_goal(&ball);
            if (goal != 0) {
                last_scorer = goal;
                if (goal == 1) p1.score++;
                else p2.score++;

                if (p1.score >= MAX_SCORE) {
                    winner = 1;
                    pause = WIN_PAUSE_FRAMES;
                } else if (p2.score >= MAX_SCORE) {
                    winner = 2;
                    pause = WIN_PAUSE_FRAMES;
                } else {
                    pause = GOAL_PAUSE_FRAMES;
                }
            }
        } else if (pause > 0) {
            pause--;
            if (pause == 0) {
                if (winner != 0) {
                    p1.score = 0;
                    p2.score = 0;
                    winner = 0;
                    last_scorer = 0;
                    reset_positions(&p1, &p2, &ball, 1);
                } else {
                    /* Loser of the last goal gets the kickoff. */
                    reset_positions(&p1, &p2, &ball, last_scorer == 1 ? -1 : 1);
                    last_scorer = 0;
                }
            }
        }

        render_world(&p1, &p2, &ball);
        draw_hud(&p1, &p2, winner, (winner == 0) ? pause : 0);
        video_display_on_vblank();
    }

    return 0;
}
