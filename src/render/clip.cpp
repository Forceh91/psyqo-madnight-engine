/* Polygon clip detection code
 *
 * The polygon clipping logic is based on the Cohen-Sutherland algorithm, but
 * only the off-screen detection logic is used to determine which polygon edges
 * are off-screen.
 *
 * In tri_clip, the following edges are checked as follows:
 *
 *  |\
 *  |  \
 *  |    \
 *  |      \
 *  |--------
 *
 * In quad_clip, the following edges are checked as follows:
 *
 *  |---------|
 *  | \     / |
 *  |   \ /   |
 *  |   / \   |
 *  | /     \ |
 *  |---------|
 *
 * The inner portion of the quad is checked, otherwise the quad will be
 * culled out if the camera faces right into it, where all four edges
 * are off-screen at once.
 *
 */

#include "clip.hh"

#define CLIP_LEFT 1
#define CLIP_RIGHT 2
#define CLIP_TOP 4
#define CLIP_BOTTOM 8

int test_clip(const psyqo::Rect *clip, uint16_t x, uint16_t y)
{

    // Tests which corners of the screen a point lies outside of

    int result = 0;

    if (x < clip->pos.x)
    {
        result |= CLIP_LEFT;
    }

    if (x >= (clip->pos.x + (clip->size.w - 1)))
    {
        result |= CLIP_RIGHT;
    }

    if (y < clip->pos.y)
    {
        result |= CLIP_TOP;
    }

    if (y >= (clip->pos.y + (clip->size.h - 1)))
    {
        result |= CLIP_BOTTOM;
    }

    return result;
}

int quad_clip(const psyqo::Rect *clip, psyqo::Vertex *v0, psyqo::Vertex *v1, psyqo::Vertex *v2, psyqo::Vertex *v3)
{

    // Returns non-zero if a quad is outside the screen boundaries

    short c[4];

    c[0] = test_clip(clip, v0->x, v0->y);
    c[1] = test_clip(clip, v1->x, v1->y);
    c[2] = test_clip(clip, v2->x, v2->y);
    c[3] = test_clip(clip, v3->x, v3->y);

    if ((c[0] & c[1]) == 0)
        return 0;
    if ((c[1] & c[2]) == 0)
        return 0;
    if ((c[2] & c[3]) == 0)
        return 0;
    if ((c[3] & c[0]) == 0)
        return 0;
    if ((c[0] & c[2]) == 0)
        return 0;
    if ((c[1] & c[3]) == 0)
        return 0;

    return 1;
}

int tri_clip(psyqo::Rect *clip, psyqo::Vertex *v0, psyqo::Vertex *v1, psyqo::Vertex *v2)
{

    // Returns non-zero if a triangle is outside the screen boundaries

    short c[3];

    c[0] = test_clip(clip, v0->x, v0->y);
    c[1] = test_clip(clip, v1->x, v1->y);
    c[2] = test_clip(clip, v2->x, v2->y);

    if ((c[0] & c[1]) == 0)
        return 0;
    if ((c[1] & c[2]) == 0)
        return 0;
    if ((c[2] & c[0]) == 0)
        return 0;

    return 1;
}
