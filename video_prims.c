// video_prims.c
#include "video_prims.h"
#include <naomi/video.h>
#include <naomi/console.h>

// Initialize video
void vp_init(int cd) {
    video_init(cd);
}

// Clear the entire screen
void vp_clear(color_t c) {
    video_fill_screen(c);
}

// Draw a filled rectangle (hw-accelerated)
void vp_rect(int x, int y, int w, int h, color_t c) {
    // video_fill_box takes x0,y0,x1,y1
    video_fill_box(x, y, x + w - 1, y + h - 1, c);
}

// Render console text and swap buffers on vblank
void vp_vblank(void) {
    console_render();
    video_display_on_vblank();
}
