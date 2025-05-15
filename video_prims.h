// video_prims.h
#ifndef VIDEO_PRIMS_H
#define VIDEO_PRIMS_H

#include <naomi/video.h>

void vp_init(int color_depth);
void vp_clear(color_t c);
void vp_rect(int x, int y, int w, int h, color_t c);
void vp_vblank(void);

#endif
