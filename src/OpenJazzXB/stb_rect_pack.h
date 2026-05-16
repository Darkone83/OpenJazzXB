/**
 * stb_rect_pack.h  (XbJazz Phase 1 stub)
 *
 * Minimal interface-compatible stub for the STB rect packing library
 * (https://github.com/nothings/stb/blob/master/stb_rect_pack.h).
 *
 * font.cpp includes this to pack glyph rectangles into a texture atlas.
 * The Phase 1 stub uses a trivial left-to-right packer -- correct enough
 * to compile and link.  Replace with the real stb_rect_pack.h for Phase 3
 * once font rendering is being tested.
 *
 * The real file is public domain by Sean Barrett.
 * Download from: https://github.com/nothings/stb/blob/master/stb_rect_pack.h
 */

#ifndef STB_INCLUDE_STB_RECT_PACK_H
#define STB_INCLUDE_STB_RECT_PACK_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef int stbrp_coord;

    typedef struct stbrp_rect {
        int            id;
        stbrp_coord    w, h;
        stbrp_coord    x, y;
        int            was_packed;
    } stbrp_rect;

    typedef struct stbrp_node {
        stbrp_coord    x, y;
        struct stbrp_node* next;
    } stbrp_node;

    typedef struct stbrp_context {
        int width, height;
        int x, y, row_h;
    } stbrp_context;

    static void stbrp_init_target(stbrp_context* ctx, int w, int h,
        stbrp_node* nodes, int num_nodes) {
        (void)nodes; (void)num_nodes;
        ctx->width = w;
        ctx->height = h;
        ctx->x = 0;
        ctx->y = 0;
        ctx->row_h = 0;
    }

    static int stbrp_pack_rects(stbrp_context* ctx, stbrp_rect* rects, int num_rects) {
        for (int i = 0; i < num_rects; i++) {
            if (ctx->x + rects[i].w > ctx->width) {
                ctx->x = 0;
                ctx->y += ctx->row_h;
                ctx->row_h = 0;
            }
            if (ctx->y + rects[i].h > ctx->height) {
                rects[i].was_packed = 0;
                continue;
            }
            rects[i].x = ctx->x;
            rects[i].y = ctx->y;
            rects[i].was_packed = 1;
            if (rects[i].h > ctx->row_h) ctx->row_h = rects[i].h;
            ctx->x += rects[i].w;
        }
        return 1;
    }

    /* stb_rect_pack.h normally requires one TU to define
       STB_RECT_PACK_IMPLEMENTATION. Our stub has the implementation
       inline (static), so no separate definition step is needed. */
#define STB_RECT_PACK_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif /* STB_INCLUDE_STB_RECT_PACK_H */