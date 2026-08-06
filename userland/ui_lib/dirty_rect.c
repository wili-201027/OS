// PHASE 2.1: DIRTY RECTANGLE RENDERING - Implementación
// Tracking de regiones que necesitan redrawing

#include "dirty_rect.h"
#include "../libc/string.h"

// Funciones helper internas
static int rects_overlap(const Rect *r1, const Rect *r2) {
    return !(r1->x + r1->w < r2->x ||
             r2->x + r2->w < r1->x ||
             r1->y + r1->h < r2->y ||
             r2->y + r2->h < r1->y);
}

static Rect rect_union(const Rect *r1, const Rect *r2) {
    Rect result;
    result.x = (r1->x < r2->x) ? r1->x : r2->x;
    result.y = (r1->y < r2->y) ? r1->y : r2->y;
    
    int x1 = (r1->x + r1->w > r2->x + r2->w) ? r1->x + r1->w : r2->x + r2->w;
    int y1 = (r1->y + r1->h > r2->y + r2->h) ? r1->y + r1->h : r2->y + r2->h;
    
    result.w = x1 - result.x;
    result.h = y1 - result.y;
    
    return result;
}

void dirty_rect_init(DirtyRects *dr) {
    if (!dr) return;
    dr->count = 0;
    memset(dr->rects, 0, sizeof(dr->rects));
}

void dirty_rect_mark(DirtyRects *dr, int x, int y, int w, int h) {
    if (!dr || w <= 0 || h <= 0) return;
    
    Rect new_rect = {x, y, w, h};
    
    // Intentar combinar con rects existentes
    for(int i = 0; i < dr->count; i++) {
        if(rects_overlap(&dr->rects[i], &new_rect)) {
            // Expandir rect existente para incluir new_rect
            dr->rects[i] = rect_union(&dr->rects[i], &new_rect);
            return;
        }
    }
    
    // Nuevo rect (no se sobrelapaba con ninguno)
    if(dr->count < DIRTY_RECTS_MAX) {
        dr->rects[dr->count] = new_rect;
        dr->count++;
    } else {
        // Si estamos lleno, expandir el último (worst case)
        dr->rects[DIRTY_RECTS_MAX - 1] = 
            rect_union(&dr->rects[DIRTY_RECTS_MAX - 1], &new_rect);
    }
}

void dirty_rect_reset(DirtyRects *dr) {
    if (!dr) return;
    dr->count = 0;
}

int dirty_rect_is_dirty(const DirtyRects *dr) {
    if (!dr) return 0;
    return dr->count > 0;
}

int dirty_rect_count(const DirtyRects *dr) {
    if (!dr) return 0;
    return dr->count;
}

Rect dirty_rect_get(const DirtyRects *dr, int index) {
    Rect empty = {0, 0, 0, 0};
    if (!dr || index < 0 || index >= dr->count) return empty;
    return dr->rects[index];
}
