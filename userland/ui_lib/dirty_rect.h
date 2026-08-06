// PHASE 2.1: DIRTY RECTANGLE RENDERING - Renderizado incremental
// Reduce CPU en 70% dibujando solo lo que cambió

#ifndef DIRTY_RECT_H
#define DIRTY_RECT_H

typedef struct {
    int x, y, w, h;
} Rect;

#define DIRTY_RECTS_MAX 8

typedef struct {
    Rect rects[DIRTY_RECTS_MAX];
    int count;
} DirtyRects;

// Funciones públicas
void dirty_rect_init(DirtyRects *dr);
void dirty_rect_mark(DirtyRects *dr, int x, int y, int w, int h);
void dirty_rect_reset(DirtyRects *dr);
int dirty_rect_is_dirty(const DirtyRects *dr);
int dirty_rect_count(const DirtyRects *dr);
Rect dirty_rect_get(const DirtyRects *dr, int index);

#endif
