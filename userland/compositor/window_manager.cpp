// userland/compositor/window_manager.cpp
// WM glassmorphism complet. Corre en ring-0 sense syscalls.
// Finestres amb: box-blur + tint + contingut RGBA + decoració + cursor

#include <stdint.h>
#include <stddef.h>
#include "window_manager.h"

// Helper implementations needed by static theme initializers
static inline uint8_t clamp8(int v){ return (uint8_t)(v<0?0:v>255?255:v); }
static inline uint32_t rgb(uint8_t r,uint8_t g,uint8_t b){ return 0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b; }
static inline uint32_t ablend(uint32_t dst,uint32_t src,uint8_t a){
    uint32_t ia = 256 - a;
    uint32_t dr = ((dst>>16)&0xFF), dg = ((dst>>8)&0xFF), db = (dst&0xFF);
    uint32_t sr = ((src>>16)&0xFF), sg = ((src>>8)&0xFF), sb = (src&0xFF);
    uint32_t rr = (dr*ia + sr*a) >> 8;
    uint32_t rg = (dg*ia + sg*a) >> 8;
    uint32_t rb = (db*ia + sb*a) >> 8;
    return 0xFF000000u | ((rr&0xFF)<<16) | ((rg&0xFF)<<8) | (rb&0xFF);
}

// Serial helpers for diagnostics
static inline void outb(uint16_t port, uint8_t val) { (void)port; (void)val; }
static inline uint8_t inb(uint16_t port) { (void)port; return 0; }
static void serial_c(char c) { (void)c; }
static void serial_s(const char *s) { (void)s; }
static void serial_x(uint64_t v) { (void)v; }

extern "C" {
    void *malloc(uint32_t);
    void  free(void*);
    uint32_t fb_get_width(void);
    uint32_t fb_get_height(void);
    uint64_t scheduler_get_ticks(void);
    void compositor_mark_dirty(void);
    void draw_string_fb_scaled(uint32_t*, uint32_t, uint32_t, int, int, const char*, uint32_t, int);
    void wm_clear_window(void *win, uint32_t color);
    void wm_write(void *win, int x, int y, const char *text, uint32_t color);
    void wm_fill_rect(void *win, int x, int y, int w, int h, uint32_t color);
}

struct Element;
struct Window;

struct Element {
    char tag[16];
    char id[32];
    uint32_t style_flags;
    int x,y,w,h;
    struct Element *first_child;
    struct Element *next_sibling;
    struct Element *parent;
};

static Element *create_element_internal(const char *tag, const char *id);

// DOM API implementations (C exports)
extern "C" wm_element* wm_create_element(const char *tag, const char *id){
    return (wm_element*)create_element_internal(tag,id);
}

extern "C" void wm_append_child(wm_element *parent, wm_element *child){
    if(!parent || !child) return;
    Element *p = (Element*)parent; Element *c = (Element*)child;
    c->parent = p;
    if(!p->first_child) p->first_child = c;
    else {
        Element *it = p->first_child; while(it->next_sibling) it = it->next_sibling; it->next_sibling = c;
    }
}

extern "C" void wm_set_element_style(wm_element *el, uint32_t style_flags){ if(!el) return; ((Element*)el)->style_flags = style_flags; }

static int wm_strcmp(const char *a, const char *b);

struct Window {
    int      x, y, w, h;
    int      alloc_w, alloc_h; // actual size of the pixels buffer; can differ
                                // from w,h transiently during spawn/close animations
    uint32_t *pixels;
    const char *title;
    bool     visible, focused, minimised;
    bool     zombie;
    uint32_t style_flags;
    Window  *next;
    Window  *prev;
    Window  *next_free;
    int      anim_type;
    uint64_t anim_start;
    uint64_t anim_duration;
    int      anim_from_x, anim_from_y, anim_from_w, anim_from_h;
    int      anim_to_x, anim_to_y, anim_to_w, anim_to_h;
    struct Element *root;
    struct Element *titlebar;
    struct Element *content;
    struct Element *controls;
};

static Element *find_element_by_id_internal(Element *root, const char *id){
    if(!root || !id) return nullptr;
    if(root->id[0] && wm_strcmp(root->id, id)==0) return root;
    for(Element *c = root->first_child; c; c = c->next_sibling){ Element *f = find_element_by_id_internal(c,id); if(f) return f; }
    return nullptr;
}

extern "C" wm_element* wm_find_element_by_id(void *wv, const char *id){ if(!wv || !id) return nullptr; Window *w=(Window*)wv; return (wm_element*)find_element_by_id_internal(w->root,id); }

static int wm_strcmp(const char *a, const char *b){
    if(!a || !b) return a==b ? 0 : (a ? 1 : -1);
    while(*a && *a == *b){ ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void focus_window(Window *win);
extern "C" void *wm_create_window(int x,int y,int w,int h,const char*title);

// ─── Constants ────────────────────────────────────────────────────────────────
#define TITLEBAR_H   30
#define BLUR_R        4
#define BLUR_PASS     1
#define BTN_R         6
#define RESIZE_Z      8
#define MIN_WIN_W    160
#define MIN_WIN_H     96

// ─── Visual theme abstraction ──────────────────────────────────────────────
struct VisualTheme {
    const char *name;
    uint32_t titlebar_bg;
    uint32_t titlebar_border;
    uint32_t body_bg;
    uint32_t body_tint;
    uint32_t border_idle;
    uint32_t border_focus;
    uint32_t accent;
    uint32_t title_text;
    uint32_t control_bg;
    uint32_t control_border;
    uint8_t titlebar_alpha;
    uint8_t body_alpha;
};

static const VisualTheme s_theme_dark = {
    "dark",
    rgb(8, 16, 32),
    rgb(20, 90, 140),
    rgb(10, 22, 38),
    rgb(10, 22, 38),
    rgb(60, 80, 140),
    rgb(100, 160, 255),
    rgb(70, 220, 140),
    rgb(232, 242, 255),
    rgb(10, 14, 22),
    rgb(35, 70, 120),
    185,
    128,
};

static const VisualTheme s_theme_glass = {
    "glass",
    rgb(18, 28, 48),
    rgb(80, 160, 220),
    rgb(12, 18, 32),
    rgb(22, 34, 54),
    rgb(90, 120, 180),
    rgb(120, 200, 255),
    rgb(120, 240, 180),
    rgb(245, 248, 255),
    rgb(14, 20, 34),
    rgb(90, 140, 220),
    170,
    110,
};

static const VisualTheme s_theme_mint = {
    "mint",
    rgb(12, 28, 24),
    rgb(40, 140, 90),
    rgb(8, 18, 16),
    rgb(16, 32, 24),
    rgb(60, 100, 90),
    rgb(70, 220, 140),
    rgb(140, 255, 190),
    rgb(230, 255, 240),
    rgb(10, 18, 16),
    rgb(60, 120, 90),
    180,
    120,
};

static const VisualTheme *s_active_theme = &s_theme_dark;

static const VisualTheme &get_active_theme() {
    return *s_active_theme;
}

// ─── Estat global ─────────────────────────────────────────────────────────────
// Scene Graph head (front-most window)
static Window *sg_head    = nullptr;
static Window *s_focused = nullptr;
static Window *s_pending_free = nullptr; // windows waiting to be freed after render
static int     s_mx=400, s_my=300;
static bool    s_btn=false, s_drag=false, s_resize=false;
static Window *s_drag_win=nullptr, *s_resize_win=nullptr;
static int     s_dox=0, s_doy=0, s_rsx=0, s_rsy=0, s_rsw=0, s_rsh=0;
static bool    s_menu_open=false;
static int     s_menu_selection=0;
static Window *s_taskbar = nullptr;

// Per-window style flags
#define W_STYLE_GLASS   0x1
#define W_STYLE_NO_DECOR 0x2

// ─── Helpers ──────────────────────────────────────────────────────────────────
/* forward declarations for helpers used in static theme initializers */
// helpers already defined above

static Element *create_element_internal(const char *tag, const char *id){
    Element *e = (Element*)malloc(sizeof(Element));
    if(!e) return nullptr;
    for(int i=0;i<16;++i) e->tag[i]=0;
    for(int i=0;i<32;++i) e->id[i]=0;
    if(tag){ for(int i=0;i<15 && tag[i]; ++i) e->tag[i]=tag[i]; }
    if(id){ for(int i=0;i<31 && id[i]; ++i) e->id[i]=id[i]; }
    e->style_flags=0; e->x=e->y=e->w=e->h=0; e->first_child=nullptr; e->next_sibling=nullptr; e->parent=nullptr;
    return e;
}

// Public wrappers (C linkage below)
static void fill_circle(uint32_t*fb,uint32_t fbw,uint32_t fbh,
                         int cx,int cy,int r,uint32_t c){
    for(int dy=-r;dy<=r;++dy) for(int dx=-r;dx<=r;++dx)
        if(dx*dx+dy*dy<=r*r){ int px=cx+dx,py=cy+dy;
          if(px>=0&&(uint32_t)px<fbw&&py>=0&&(uint32_t)py<fbh) fb[py*fbw+px]=c; }
}

static void blend_rect(uint32_t*fb,uint32_t fbw,uint32_t fbh,
                        int x,int y,int w,int h,uint32_t sc,uint8_t a){
    for(int r=y;r<y+h;++r){ if(r<0||(uint32_t)r>=fbh) continue;
      for(int c=x;c<x+w;++c){ if(c<0||(uint32_t)c>=fbw) continue;
        fb[r*fbw+c]=ablend(fb[r*fbw+c],sc,a); } }
}

static void clamp_window_to_screen(Window *win, uint32_t fbw, uint32_t fbh){
    if(!win || !fbw || !fbh) return;

    int max_w = (int)fbw - 16;
    int max_h = (int)fbh - TITLEBAR_H - 16;
    if (max_w < MIN_WIN_W) max_w = MIN_WIN_W;
    if (max_h < MIN_WIN_H) max_h = MIN_WIN_H;

    if (win->w < MIN_WIN_W) win->w = MIN_WIN_W;
    if (win->h < MIN_WIN_H) win->h = MIN_WIN_H;
    if (win->w > max_w) win->w = max_w;
    if (win->h > max_h) win->h = max_h;

    if (win->x < 8) win->x = 8;
    if (win->y < TITLEBAR_H + 4) win->y = TITLEBAR_H + 4;
    if (win->x + win->w > (int)fbw - 8) win->x = (int)fbw - win->w - 8;
    if (win->y + win->h > (int)fbh - 8) win->y = (int)fbh - win->h - 8;
}

// --- Capture and simple box-blur utilities (two-pass separable blur)
static uint32_t *capture_region(uint32_t *fb, uint32_t fbw, uint32_t fbh,
                                int x,int y,int w,int h)
{
    if (!fb || w<=0 || h<=0) return nullptr;
    // limit capture size to avoid huge allocations
    if ((uint64_t)w * (uint64_t)h > 1024u*1024u) return nullptr;
    uint32_t *buf = (uint32_t*)malloc((uint32_t)(w * h * 4));
    if(!buf) return nullptr;
    for(int row=0; row<h; ++row){
        int fy = y + row;
        if(fy < 0 || (uint32_t)fy >= fbh){
            for(int col=0; col<w; ++col) buf[row*w+col] = 0;
            continue;
        }
        for(int col=0; col<w; ++col){
            int fx = x + col;
            if(fx < 0 || (uint32_t)fx >= fbw) buf[row*w+col] = 0;
            else buf[row*w+col] = fb[fy*fbw + fx];
        }
    }
    return buf;
}

static void box_blur_separable(uint32_t *buf, uint32_t *tmp, int w, int h, int r)
{
    if(!buf || !tmp) return;
    int diameter = r*2+1;
    // horizontal pass
    for(int y=0;y<h;++y){
        for(int x=0;x<w;++x){
            uint32_t ra=0,ga=0,ba=0,ca=0;
            int cnt=0;
            int sx = x - r; if(sx<0) sx=0;
            int ex = x + r; if(ex>=w) ex=w-1;
            for(int xi=sx; xi<=ex; ++xi){ uint32_t p = buf[y*w + xi]; ca += (p>>24)&0xFF; ra += (p>>16)&0xFF; ga += (p>>8)&0xFF; ba += p & 0xFF; ++cnt; }
            tmp[y*w + x] = ( (uint32_t)(ca/cnt) << 24) | ((uint32_t)(ra/cnt)<<16) | ((uint32_t)(ga/cnt)<<8) | (uint32_t)(ba/cnt);
        }
    }
    // vertical pass
    for(int y=0;y<h;++y){
        for(int x=0;x<w;++x){
            uint32_t ra=0,ga=0,ba=0,ca=0;
            int cnt=0;
            int sy = y - r; if(sy<0) sy=0;
            int ey = y + r; if(ey>=h) ey=h-1;
            for(int yi=sy; yi<=ey; ++yi){ uint32_t p = tmp[yi*w + x]; ca += (p>>24)&0xFF; ra += (p>>16)&0xFF; ga += (p>>8)&0xFF; ba += p & 0xFF; ++cnt; }
            buf[y*w + x] = ( (uint32_t)(ca/cnt) << 24) | ((uint32_t)(ra/cnt)<<16) | ((uint32_t)(ga/cnt)<<8) | (uint32_t)(ba/cnt);
        }
    }
}

static void blend_buf_to_fb(uint32_t *fb, uint32_t fbw, uint32_t fbh,
                            uint32_t *buf, int bx,int by,int w,int h, uint8_t alpha)
{
    if(!fb || !buf) return;
    for(int row=0; row<h; ++row){
        int fy = by + row;
        if(fy<0 || (uint32_t)fy>=fbh) continue;
        for(int col=0; col<w; ++col){
            int fx = bx + col;
            if(fx<0 || (uint32_t)fx>=fbw) continue;
            uint32_t src = buf[row*w + col];
            fb[fy*fbw + fx] = ablend(fb[fy*fbw + fx], src, alpha);
        }
    }
}

// CRITICAL FIX: the window's pixel buffer was only ever allocated once, at
// creation time, for the window's initial size. Resizing a window (drag from
// the corner) changed win->w/win->h but never touched win->pixels -- so any
// subsequent wm_clear_window/wm_fill_rect/wm_write, or the content-blit in
// render_window, indexed past the end of a buffer that was too small,
// reading (and sometimes writing) heap memory that didn't belong to the
// window. That out-of-bounds access is the root cause of windows visually
// corrupting or the system crashing after a resize. This reallocates the
// backing buffer to match, preserving whatever content overlaps the old and
// new size.
static bool resize_window_pixels(Window *win, int new_w, int new_h){
    if(!win || new_w<=0 || new_h<=0) return false;
    if(win->pixels && win->alloc_w==new_w && win->alloc_h==new_h) return true;

    uint32_t *new_buf = (uint32_t*)malloc((uint32_t)(new_w*new_h*4));
    if(!new_buf) return false; // allocation failed: keep old buffer, caller must not commit new dims

    for(int row=0; row<new_h; ++row){
        uint32_t shade = (uint32_t)((row * 255) / (new_h > 0 ? new_h : 1));
        uint8_t base = (uint8_t)(16 + (shade >> 2));
        uint8_t blue = (uint8_t)(30 + (shade >> 1));
        uint8_t green = (uint8_t)(8 + (shade / 6));
        uint32_t pattern = rgb(base, green + 6, blue + 10);
        for(int col=0; col<new_w; ++col) new_buf[row*new_w+col] = pattern;
    }

    if(win->pixels && win->alloc_w>0 && win->alloc_h>0){
        int copy_w = win->alloc_w < new_w ? win->alloc_w : new_w;
        int copy_h = win->alloc_h < new_h ? win->alloc_h : new_h;
        for(int row=0; row<copy_h; ++row)
            for(int col=0; col<copy_w; ++col)
                new_buf[row*new_w+col] = win->pixels[row*win->alloc_w+col];
    }

    if(win->pixels) free(win->pixels);
    win->pixels = new_buf;
    win->alloc_w = new_w;
    win->alloc_h = new_h;
    return true;
}

static Window *find_window_by_title(const char *title){
    for(Window *w=sg_head; w; w=w->next){
        if(w->title && title && wm_strcmp(w->title, title) == 0) return w;
    }
    return nullptr;
}

static void open_dock_app(const char *title){
    Window *existing = find_window_by_title(title);
    if(existing){
        focus_window(existing);
        return;
    }

    if(wm_strcmp(title, "TERMINAL") == 0){
        void *win = wm_create_window(80, 80, 520, 340, "TERMINAL");
        if(win){
            wm_clear_window(win, 0xFF07131F);
            wm_fill_rect(win, 10, 10, 500, 1, 0xFF3D6DFF);
            wm_write(win, 15, 20, "GPT-OS TERMINAL", 0xFFEAF6FF);
            wm_write(win, 15, 40, "Interactive dock launcher", 0xFF8DC6FF);
        }
    } else if(wm_strcmp(title, "SYSTEM MONITOR") == 0){
        void *win = wm_create_window(140, 120, 380, 280, "SYSTEM MONITOR");
        if(win){
            wm_clear_window(win, 0xFF091422);
            wm_fill_rect(win, 10, 10, 360, 1, 0xFF4B86FF);
            wm_write(win, 15, 20, "SYSTEM STATUS", 0xFFEAF6FF);
            wm_write(win, 15, 42, "Dock interaction enabled", 0xFF5CFFB2);
        }
    } else if(wm_strcmp(title, "FILES") == 0){
        void *win = wm_create_window(220, 160, 420, 300, "FILES");
        if(win){
            wm_clear_window(win, 0xFF08121C);
            wm_fill_rect(win, 10, 10, 400, 1, 0xFF4B86FF);
            wm_write(win, 15, 20, "FILE MANAGER", 0xFFEAF6FF);
            wm_write(win, 15, 45, "Dock app launched", 0xFF8DC6FF);
        }
    }
}

static void render_menu_overlay(uint32_t*fb,uint32_t fbw,uint32_t fbh){
    if(!fb || !fbw || !fbh) return;

    int panel_w = (int)fbw - 200;
    int panel_h = 180;
    int panel_x = 100;
    int panel_y = 60;
    if (panel_w < 320) panel_w = 320;
    if (panel_h < 140) panel_h = 140;
    if (panel_x + panel_w > (int)fbw) panel_x = (int)fbw - panel_w - 20;

    blend_rect(fb,fbw,fbh,panel_x,panel_y,panel_w,panel_h,rgb(3,8,18),170);
    blend_rect(fb,fbw,fbh,panel_x,panel_y,panel_w,2,rgb(70,220,140),110);
    blend_rect(fb,fbw,fbh,panel_x,panel_y+panel_h-2,panel_w,2,rgb(70,120,200),90);
    for(int i=0;i<panel_w;i+=24) blend_rect(fb,fbw,fbh,panel_x+i,panel_y+16,12,panel_h-32,rgb(20,60,100),45);

    draw_string_fb_scaled(fb,fbw,fbh,panel_x+24,panel_y+18,"GPT-OS MENU",rgb(232,242,255),2);
    draw_string_fb_scaled(fb,fbw,fbh,panel_x+24,panel_y+70,"[1] TERMINAL",s_menu_selection==0?rgb(70,220,140):rgb(180,220,255),1);
    draw_string_fb_scaled(fb,fbw,fbh,panel_x+24,panel_y+96,"[2] SYSTEM",s_menu_selection==1?rgb(70,220,140):rgb(180,220,255),1);
    draw_string_fb_scaled(fb,fbw,fbh,panel_x+24,panel_y+122,"[3] FILES",s_menu_selection==2?rgb(70,220,140):rgb(180,220,255),1);
    draw_string_fb_scaled(fb,fbw,fbh,panel_x+24,panel_y+148,"Windows = close menu  Alt+Tab = focus",rgb(120,170,220),1);
}

// Font 3×5 extern
extern "C" void draw_string_fb(uint32_t*,uint32_t,uint32_t,int,int,const char*,uint32_t);

// ─── Render d'una finestra ────────────────────────────────────────────────────
static void render_window(uint32_t*fb,uint32_t fbw,uint32_t fbh,
                           Window*win,
                           void(*blur)(uint32_t*,uint32_t,int,int,int,int,int))
{
    if(!win || win->zombie || !win->visible || win->minimised) return;

    /* Defensive: ensure pixel buffer and dimensions are valid to avoid
     * reading/writing out-of-bounds if the window was partially freed or
     * corrupted. Skip rendering that window if invalid. */
    if (!win->pixels || win->w <= 0 || win->h <= 0) return;

    const VisualTheme &theme = get_active_theme();
    int wx=win->x, wy=win->y-TITLEBAR_H;
    int ww=win->w, wh=win->h+TITLEBAR_H;

    // 1. Background blur behind the window. GLASS and plain windows now share
    // the same allocation-free in-place blur (blur_fn keeps one reusable
    // static scratch buffer in glass_renderer.cpp, sized once and reused).
    // The old GLASS path did a fresh malloc+free capture AND a second,
    // redundant hand-rolled blur pass every single frame for every glass
    // window -- with two demo windows spawned as GLASS by default, that was
    // the single biggest cost in the render loop and the main reason FPS
    // dropped. The titlebar/body tint blended in step 2 below already gives
    // the glass look, so no separate capture buffer is needed.
    for(int p=0;p<BLUR_PASS;++p) blur(fb,fbw,wx,wy,ww,wh,BLUR_R);

    // 2. Draw elements from DOM: titlebar, controls, content
    Element *el_root = win->root;
    Element *el_title = win->titlebar;
    Element *el_content = win->content;
    Element *el_controls = win->controls;

    int root_gx = wx + (el_root ? el_root->x : 0);
    int root_gy = wy + (el_root ? el_root->y : 0);

    int tbx = wx + (el_title ? el_title->x : 0);
    int tby = wy + (el_title ? el_title->y : 0);
    int tbw = el_title ? el_title->w : ww;
    int tbh = el_title ? el_title->h : TITLEBAR_H;

    // Titlebar background and border
    blend_rect(fb, fbw, fbh, tbx, tby, tbw, tbh, theme.titlebar_bg, theme.titlebar_alpha);
    blend_rect(fb, fbw, fbh, tbx, tby + tbh - 2, tbw, 2, theme.titlebar_border, 110);

    // Content/background tint
    int cbx = wx + (el_content ? el_content->x : 0);
    int cby = wy + (el_content ? el_content->y : TITLEBAR_H);
    int cbw = el_content ? el_content->w : ww;
    int cbh = el_content ? el_content->h : win->h;
    blend_rect(fb, fbw, fbh, cbx, cby, cbw, cbh, theme.body_bg, theme.body_alpha);

    // 3. Contingut propi (ARGB)
    // The pixel buffer's real stride is alloc_w/alloc_h, not win->w/win->h --
    // those only match outside of spawn/close animations, where win->w/h are
    // temporarily smaller than the allocated buffer. Indexing with win->w as
    // the stride while the buffer was laid out with alloc_w produced visibly
    // garbled/"broken" window contents while animating.
    int content_w = win->alloc_w < cbw ? win->alloc_w : cbw;
    int content_h = win->alloc_h < cbh ? win->alloc_h : cbh;
    uint64_t win_area = (uint64_t)win->alloc_w * (uint64_t)win->alloc_h;
    for(int row=0; row<content_h; ++row) {
        int fy = cby + row;
        if (fy < 0 || (uint32_t)fy >= fbh) continue;
        uint64_t row_base = (uint64_t)row * (uint64_t)win->alloc_w;
        if (row_base >= win_area) continue;
        for(int col=0; col<content_w; ++col) {
            int fx = cbx + col;
            if (fx < 0 || (uint32_t)fx >= fbw) continue;
            uint64_t idx = row_base + (uint64_t)col;
            if (idx >= win_area) continue;
            uint32_t p = win->pixels[idx];
            uint8_t a = (uint8_t)((p >> 24) & 0xFF);
            if (!a) continue;
            fb[fy * fbw + fx] = ablend(fb[fy * fbw + fx], p, a);
        }
    }

    // 5. Vora brillant
    uint32_t borcol = win->focused ? theme.border_focus : theme.border_idle;
    uint8_t  bora   = win->focused ? 120 : 60;
    // Vora superior
    for(int c=wx;c<wx+ww;++c) if(c>=0&&(uint32_t)c<fbw&&wy>=0&&(uint32_t)wy<fbh)
        fb[wy*fbw+c]=ablend(fb[wy*fbw+c],borcol,bora);
    // Vora inferior
    int bot=win->y+win->h-1;
    for(int c=wx;c<wx+ww;++c) if(c>=0&&(uint32_t)c<fbw&&bot>=0&&(uint32_t)bot<fbh)
        fb[bot*fbw+c]=ablend(fb[bot*fbw+c],borcol,bora);
    // Vores laterals
    for(int r=wy;r<wy+wh;++r){
        if(wx>=0&&(uint32_t)wx<fbw&&r>=0&&(uint32_t)r<fbh)
            fb[r*fbw+wx]=ablend(fb[r*fbw+wx],borcol,bora);
        int rg=wx+ww-1;
        if(rg>=0&&(uint32_t)rg<fbw&&r>=0&&(uint32_t)r<fbh)
            fb[r*fbw+rg]=ablend(fb[r*fbw+rg],borcol,bora);
    }

    // 6. Focus accent
    if(win->focused){
        for(int c=wx+1;c<wx+ww-1;++c)
            if(c>=0&&(uint32_t)c<fbw&&wy>=0&&(uint32_t)wy<fbh)
                fb[wy*fbw+c]=ablend(fb[wy*fbw+c],theme.accent,90);
    }

    // 7. Minimal title controls: use controls element if present
    int ctrl_y = tby + tbh/2;
    int ctrl_base_x = wx + 14;
    if (el_controls) ctrl_base_x = wx + el_controls->x + 6;
    for(int i=0;i<3;++i){
        int cx = ctrl_base_x + i*18;
        if(cx>=0&&(uint32_t)cx<fbw&&ctrl_y>=0&&(uint32_t)ctrl_y<fbh){
            fill_circle(fb, fbw, fbh, cx, ctrl_y, BTN_R-3, theme.control_bg);
            fb[(ctrl_y-1)*fbw+cx]=theme.control_border;
        }
    }

    // 8. Title text
    if(win->title){
        int tx = tbx + (el_controls ? (el_controls->x + el_controls->w + 12) : 42);
        int ty = tby + 6;
        draw_string_fb_scaled(fb, fbw, fbh, tx, ty, win->title, theme.title_text, 1);
    }

    // 9. Handle resize (triangle baix-dret)
    for(int k=0;k<RESIZE_Z;++k){
        int rx=wx+ww-1-k;
        for(int j=0;j<=k;++j){
            int ry=win->y+win->h-1-j;
            if(rx>=0&&(uint32_t)rx<fbw&&ry>=0&&(uint32_t)ry<fbh)
                fb[ry*fbw+rx]=ablend(fb[ry*fbw+rx],rgb(140,170,220),80);
        }
    }
}

extern "C"
void wm_set_theme(int theme_id)
{
    switch(theme_id){
        case 1: s_active_theme = &s_theme_glass; break;
        case 2: s_active_theme = &s_theme_mint; break;
        default: s_active_theme = &s_theme_dark; break;
    }
    compositor_mark_dirty();
}

// ─── API pública ──────────────────────────────────────────────────────────────
extern "C"
void wm_render_all(uint32_t*fb,uint32_t w,uint32_t h,
                   void(*blur)(uint32_t*,uint32_t,int,int,int,int,int))
{
    Window *tail = sg_head;
    while (tail && tail->next) tail = tail->next;

    Window *win = tail;
    int iter = 0;
    while (win) {
        Window *prev = win->prev;
        render_window(fb, w, h, win, blur);
        win = prev;
        ++iter;
        if (iter > 1024) { serial_s("[WM] render: too many windows, aborting loop\n"); break; }
    }

    if (s_menu_open) render_menu_overlay(fb, w, h);

    // After rendering, free any windows that were deferred for safe destruction
    while (s_pending_free) {
        Window *fw = s_pending_free;
        s_pending_free = fw->next_free;
        serial_s("[WM] freeing deferred win="); serial_x((uint64_t)fw); serial_s(" pixels="); serial_x((uint64_t)fw->pixels); serial_s("\n");
        if (s_taskbar == fw) s_taskbar = nullptr;
        if (s_focused == fw) s_focused = nullptr;
        if (fw->pixels) { free(fw->pixels); fw->pixels = nullptr; }
        free(fw);
    }
}

extern "C"
void *wm_create_window(int x,int y,int w,int h,const char*title)
{
    Window *win=(Window*)malloc(sizeof(Window));
    if(!win) return nullptr;
    win->x=x; win->y=y+TITLEBAR_H; win->w=w; win->h=h;
    win->alloc_w=w; win->alloc_h=h;
    win->pixels=(uint32_t*)malloc((uint32_t)(w*h*4));
    if(win->pixels) {
        for(int row=0;row<h;++row) {
            uint32_t shade = (uint32_t)((row * 255) / (h > 0 ? h : 1));
            uint8_t base = (uint8_t)(16 + (shade >> 2));
            uint8_t blue = (uint8_t)(30 + (shade >> 1));
            uint8_t green = (uint8_t)(8 + (shade / 6));
            uint32_t pattern = rgb(base, green + 6, blue + 10);
            for(int col=0; col<w; ++col) {
                win->pixels[row*w+col] = pattern;
            }
        }
    }
    win->title=title; win->visible=true; win->focused=false; win->minimised=false; win->zombie=false; win->style_flags=0; win->next_free = nullptr;
    // Create DOM-like element structure: root -> titlebar + content + controls
    win->root = create_element_internal("window", title ? title : "win");
    win->titlebar = create_element_internal("titlebar", "titlebar");
    win->content = create_element_internal("content", "content");
    win->controls = create_element_internal("controls", "controls");
    if(win->root){
        // set sizes relative to window
        win->root->w = w; win->root->h = h + TITLEBAR_H; win->root->x = 0; win->root->y = 0;
        if(win->titlebar){ win->titlebar->w = w; win->titlebar->h = TITLEBAR_H; win->titlebar->x = 0; win->titlebar->y = 0; win->titlebar->parent = win->root; win->titlebar->next_sibling = win->root->first_child; win->root->first_child = win->titlebar; }
        if(win->content){ win->content->w = w; win->content->h = h; win->content->x = 0; win->content->y = TITLEBAR_H; win->content->parent = win->root; win->content->next_sibling = win->root->first_child; win->root->first_child = win->content; }
        if(win->controls){ win->controls->w = 64; win->controls->h = TITLEBAR_H; win->controls->x = 8; win->controls->y = (TITLEBAR_H/2)-8; win->controls->parent = win->titlebar; win->titlebar->first_child = win->controls; }
    }
    // Insert at front of scene graph (top-most)
    win->prev = nullptr; win->next = sg_head; if(sg_head) sg_head->prev = win; sg_head = win;
    clamp_window_to_screen(win, fb_get_width(), fb_get_height());
    if (title && wm_strcmp(title, "TERMINAL") == 0) {
        s_taskbar = s_taskbar; // keep taskbar pointer stable
    }
    serial_s("[WM] created win="); serial_x((uint64_t)win); serial_s(" pixels="); serial_x((uint64_t)win->pixels); serial_s(" next="); serial_x((uint64_t)win->next); serial_s("\n");
    if(s_focused) s_focused->focused=false;
    s_focused=win; win->focused=true;
    compositor_mark_dirty();
    return win;
}

extern "C"
void wm_destroy_window(void *wv){
    Window *win=(Window*)wv; if(!win) return;
    // Unlink from scene graph list
    if (win->prev) win->prev->next = win->next; else sg_head = win->next;
    if (win->next) win->next->prev = win->prev;
    if(s_focused==win){ s_focused=sg_head; if(s_focused) s_focused->focused=true; }
    serial_s("[WM] destroy win="); serial_x((uint64_t)win); serial_s(" pixels="); serial_x((uint64_t)win->pixels); serial_s(" next="); serial_x((uint64_t)win->next); serial_s("\n");
    // Defer actual free until after render to avoid use-after-free during rendering.
    win->visible = false;
    win->zombie = true;
    // Push into pending free list
    win->next_free = s_pending_free;
    s_pending_free = win;
    compositor_mark_dirty();
}

// Close the currently focused window (for Alt+F4)
extern "C"
void wm_close_focused_window(void) {
    if (!s_focused) return;
    wm_destroy_window(s_focused);
}

static void focus_window(Window *win){
    if(!win||win==s_focused) return;
    // Bring node to front of scene graph
    if(win->prev) win->prev->next = win->next; else { /* already head? */ }
    if(win->next) win->next->prev = win->prev;
    // Insert at head
    win->prev = nullptr;
    win->next = sg_head;
    if(sg_head) sg_head->prev = win;
    sg_head = win;
    if(s_focused) s_focused->focused=false;
    s_focused=win; win->focused=true;
    compositor_mark_dirty();
}

static Window *hit_test(int mx,int my){
    for(Window *w=sg_head;w;w=w->next){
        if(!w->visible||w->minimised||w->zombie) continue;
        int wy=w->y-TITLEBAR_H;
        if(mx>=w->x&&mx<w->x+w->w&&my>=wy&&my<wy+w->h+TITLEBAR_H) return w;
    }
    return nullptr;
}

static void handle_dock_click(int ax,int ay){
    if(!s_taskbar) return;
    int tbx=s_taskbar->x;
    int tby=s_taskbar->y - TITLEBAR_H;
    if(ay < tby || ay >= tby + s_taskbar->h + TITLEBAR_H) return;

    if(ax >= tbx + 8 && ax < tbx + 92){
        open_dock_app("TERMINAL");
    } else if(ax >= tbx + 102 && ax < tbx + 182){
        open_dock_app("SYSTEM MONITOR");
    } else if(ax >= tbx + 192 && ax < tbx + 272){
        open_dock_app("FILES");
    }
}

extern "C"
void wm_register_taskbar(void *wv){
    s_taskbar = (Window*)wv;
}

extern "C"
void wm_handle_mouse(int ax,int ay,bool btn,bool pressed,bool released)
{
    s_mx=ax; s_my=ay;
    s_btn=btn;

    if (s_menu_open && pressed) {
        s_menu_open = false;
        compositor_mark_dirty();
        return;
    }

    if(pressed){
        if(s_taskbar && ay >= s_taskbar->y - TITLEBAR_H && ay < s_taskbar->y + s_taskbar->h){
            handle_dock_click(ax, ay);
            return;
        }

        Window *h=hit_test(ax,ay);
        if(h){
            focus_window(h);
            int wy=h->y-TITLEBAR_H;
            bool in_tb=(ay>=wy&&ay<wy+TITLEBAR_H);
            bool in_rs=(ax>=h->x+h->w-RESIZE_Z&&ay>=h->y+h->h-RESIZE_Z);

            if(in_rs){
                s_resize=true;
                s_resize_win=h;
                s_rsx=ax; s_rsy=ay;
                s_rsw=h->w; s_rsh=h->h;
                s_drag=false; s_drag_win=nullptr;
            }
            else if(in_tb){
                // Check if click hit a title control
                int title_top = h->y - TITLEBAR_H;
                int ctrl_y = title_top + TITLEBAR_H/2;
                bool control_handled = false;
                int ctrl_base_x = h->x + 14;
                if(h->controls){ ctrl_base_x = h->x + h->controls->x; }
                for(int i=0;i<3;++i){
                    int cx = ctrl_base_x + i*18;
                    if (ax >= cx-8 && ax <= cx+12 && ay >= ctrl_y-8 && ay <= ctrl_y+8){
                        // control i clicked: 0=max,1=min,2=close
                        if(i==1){ h->minimised = true; compositor_mark_dirty(); }
                        else if(i==2){ wm_start_close_animation(h, 500); }
                        control_handled = true;
                        break;
                    }
                }
                if(!control_handled){
                    s_drag=true;
                    s_drag_win=h;
                    s_dox = ax - h->x;
                    s_doy = ay - title_top;
                }
                s_resize=false; s_resize_win=nullptr;
            }
        }
        else {
            s_drag=false; s_drag_win=nullptr;
            s_resize=false; s_resize_win=nullptr;
        }
    }

    if(btn){
        bool moved = false;
        if(s_drag&&s_drag_win){
            int new_x = ax - s_dox;
            int new_y = ay - s_doy + TITLEBAR_H;
            if(new_x != s_drag_win->x || new_y != s_drag_win->y) {
                s_drag_win->x = new_x;
                s_drag_win->y = new_y;
                clamp_window_to_screen(s_drag_win, fb_get_width(), fb_get_height());
                moved = true;
            }
        }
        if(s_resize&&s_resize_win){
            int nw=s_rsw+(ax-s_rsx), nh=s_rsh+(ay-s_rsy);
            if(nw>=MIN_WIN_W && nw != s_resize_win->w){ s_resize_win->w=nw; moved=true; }
            if(nh>=MIN_WIN_H && nh != s_resize_win->h){ s_resize_win->h=nh; moved=true; }
            if(moved){
                clamp_window_to_screen(s_resize_win, fb_get_width(), fb_get_height());
                resize_window_pixels(s_resize_win, s_resize_win->w, s_resize_win->h);
                if(s_resize_win->root){ s_resize_win->root->w = s_resize_win->w; s_resize_win->root->h = s_resize_win->h + TITLEBAR_H; }
                if(s_resize_win->titlebar) s_resize_win->titlebar->w = s_resize_win->w;
                if(s_resize_win->content){ s_resize_win->content->w = s_resize_win->w; s_resize_win->content->h = s_resize_win->h; }
            }
        }
        if (moved) compositor_mark_dirty();
    }

    if(released){
        s_drag=false;
        s_drag_win=nullptr;
        s_resize=false;
        s_resize_win=nullptr;
    }
}

// Set per-window style flags (replace flags)
extern "C"
void wm_set_window_style(void *wv, uint32_t flags) {
    Window *win = (Window*)wv;
    if (!win) return;
    win->style_flags = flags;
    compositor_mark_dirty();
}

// --- Animation helpers and control -------------------------------------------------
static inline uint64_t ticks_to_now(void) { extern uint64_t scheduler_get_ticks(void); return scheduler_get_ticks(); }

static inline double ease_out_cubic(double t){ double u=1.0-t; return 1.0 - u*u*u; }
static inline double ease_out_quart(double t){ double u=1.0-t; return 1.0 - u*u*u*u; }

extern "C"
void wm_start_spawn_animation(void *wv, int target_w, int target_h){
    Window *win=(Window*)wv; if(!win) return;
    uint64_t now = ticks_to_now();
    win->anim_type = 1;
    win->anim_start = now + 0;
    win->anim_duration = 14;

    int collapsed_h = TITLEBAR_H + 24;
    int centered_x = win->x + (win->w - target_w) / 2;
    int centered_y = win->y + (win->h - collapsed_h) / 2;

    win->anim_from_x = centered_x;
    win->anim_from_y = centered_y;
    win->anim_from_w = target_w / 2;
    win->anim_from_h = collapsed_h;

    win->anim_to_x = win->x;
    win->anim_to_y = win->y;
    win->anim_to_w = target_w;
    win->anim_to_h = target_h;

    win->x = win->anim_from_x;
    win->y = win->anim_from_y;
    win->w = win->anim_from_w;
    win->h = win->anim_from_h;
    compositor_mark_dirty();
}

extern "C"
void wm_start_close_animation(void *wv, int collapse_ms){
    Window *win=(Window*)wv; if(!win) return;
    uint64_t now = ticks_to_now();
    win->anim_type = 2;
    win->anim_start = now;
    win->anim_duration = (collapse_ms>0)? (collapse_ms/55 + 1) : 9;

    int collapsed_h = TITLEBAR_H + 24;
    int current_center_y = win->y + (win->h - collapsed_h) / 2;

    win->anim_from_x = win->x;
    win->anim_from_y = win->y;
    win->anim_from_w = win->w;
    win->anim_from_h = win->h;

    win->anim_to_x = win->x + (win->w - (win->w / 2)) / 2;
    win->anim_to_y = current_center_y;
    win->anim_to_w = win->w / 2;
    win->anim_to_h = collapsed_h;
    compositor_mark_dirty();
}

extern "C"
void wm_tick_animations(uint64_t now_ticks){
    Window *w = sg_head;
    Window *to_destroy[16]; int td_count=0;
    while(w){
        if(w->anim_type){
            uint64_t elapsed = now_ticks - w->anim_start;
            double t = 0.0;
            if(w->anim_duration>0) t = (double)elapsed / (double)w->anim_duration;
            if(t>=1.0) t=1.0;
            double e = ease_out_quart(t);
            int nx = (int)(w->anim_from_x + (w->anim_to_x - w->anim_from_x) * e);
            int ny = (int)(w->anim_from_y + (w->anim_to_y - w->anim_from_y) * e);
            int nw = (int)(w->anim_from_w + (w->anim_to_w - w->anim_from_w) * e);
            int nh = (int)(w->anim_from_h + (w->anim_to_h - w->anim_from_h) * e);
            bool changed = false;
            if(nx != w->x){ w->x = nx; changed = true; }
            if(ny != w->y){ w->y = ny; changed = true; }
            if(nw != w->w){ w->w = nw; changed = true; }
            if(nh != w->h){ w->h = nh; changed = true; }
            if(changed) clamp_window_to_screen(w, fb_get_width(), fb_get_height()), compositor_mark_dirty();
            if(t>=1.0){
                // finish
                if(w->anim_type==2){
                    // schedule destroy
                    if(td_count < 16) to_destroy[td_count++] = w;
                }
                w->anim_type = 0;
            }
        }
        w = w->next;
    }
    for(int i=0;i<td_count;++i) wm_destroy_window(to_destroy[i]);
}

extern "C"
void wm_move_focused_window(int dx,int dy){
    if(!s_focused) return;
    s_focused->x += dx;
    s_focused->y += dy;
    clamp_window_to_screen(s_focused, fb_get_width(), fb_get_height());
    compositor_mark_dirty();
}

extern "C"
void wm_cycle_focus(int direction){
    Window *visible[32];
    int count=0;
    for(Window *w=sg_head; w && count < 32; w=w->next){
        if(w->visible && !w->minimised) visible[count++] = w;
    }
    if(count <= 1) return;
    int idx=0;
    while(idx < count && visible[idx] != s_focused) ++idx;
    if(idx >= count) idx = 0;
    int next = (idx + direction + count) % count;
    focus_window(visible[next]);
}

extern "C"
void wm_toggle_menu(void){
    s_menu_open = !s_menu_open;
    s_menu_selection = 0;
    compositor_mark_dirty();
}

extern "C"
void wm_handle_key(uint8_t sc,bool pressed)
{ 
    if(!s_focused) return;
    
    // Simple feedback: fill window with color based on key press
    uint32_t color = pressed ? rgb(100, 150, 255) : rgb(80, 100, 200);
    for(int i=0; i<s_focused->alloc_w*s_focused->alloc_h; ++i)
        s_focused->pixels[i] = color;
    compositor_mark_dirty();
}

extern "C"
void wm_get_cursor_pos(int *x, int *y){
    if(x) *x = s_mx;
    if(y) *y = s_my;
}

extern "C"
void wm_draw_cursor(uint32_t*fb,uint32_t fbw,uint32_t fbh)
{
    // Halo suau
    uint32_t gc=rgb(160,200,255);
    for(int dy=-5;dy<=5;++dy) for(int dx=-5;dx<=5;++dx){
        int d2=dx*dx+dy*dy; if(d2>25) continue;
        uint8_t a=(uint8_t)(50-d2*2);
        int px=s_mx+dx,py=s_my+dy;
        if(px>=0&&(uint32_t)px<fbw&&py>=0&&(uint32_t)py<fbh)
            fb[py*fbw+px]=ablend(fb[py*fbw+px],gc,a);
    }
    // Fletxa blanc amb ombra
    static const uint8_t arrow[11]={0xC0,0xE0,0xF0,0xF8,0xFC,0xFE,0xFC,0xCC,0x86,0x06,0x03};
    for(int row=0;row<11;++row) for(int col=0;col<8;++col){
        if(arrow[row]&(0x80>>col)){
            int px=s_mx+col,py=s_my+row;
            if(px+1<(int)fbw&&py+1<(int)fbh)
                fb[(py+1)*fbw+(px+1)]=ablend(fb[(py+1)*fbw+(px+1)],rgb(10,10,20),140);
            if(px>=0&&(uint32_t)px<fbw&&py>=0&&(uint32_t)py<fbh)
                fb[py*fbw+px]=0xFFFFFFFF;
        }
    }
}

// ─── Window Content APIs ──────────────────────────────────────────────────────

// Global window registry for syscall support
#define MAX_WINDOW_IDS 16
static void* s_window_ids[MAX_WINDOW_IDS] = {nullptr};

static int register_window(void *win) {
    for (int i = 0; i < MAX_WINDOW_IDS; ++i) {
        if (s_window_ids[i] == nullptr) {
            s_window_ids[i] = win;
            return i + 1;  // Return ID starting from 1 (0 is invalid)
        }
    }
    return 0;  // No slots available
}

static Window* get_window_by_id(int wid) {
    if (wid <= 0 || wid > MAX_WINDOW_IDS) return nullptr;
    return (Window*)s_window_ids[wid - 1];
}

// Clear window with color
extern "C"
void wm_clear_window(void *wv, uint32_t color) {
    Window *win = (Window*)wv;
    if (!win || !win->pixels || win->alloc_w <= 0 || win->alloc_h <= 0) return;
    int total = win->alloc_w * win->alloc_h;
    for (int i = 0; i < total; ++i)
        win->pixels[i] = color;
    compositor_mark_dirty();
}

// Fill window rect
extern "C"
void wm_fill_rect(void *wv, int x, int y, int w, int h, uint32_t color) {
    Window *win = (Window*)wv;
    if (!win || !win->pixels || win->alloc_w <= 0 || win->alloc_h <= 0) return;
    
    // Clamp to the ACTUAL pixel buffer bounds (alloc_w/alloc_h), not the
    // possibly-stale logical win->w/win->h, to avoid writing past the buffer.
    int y_start = (y < 0) ? 0 : y;
    int y_end = (y + h > win->alloc_h) ? win->alloc_h : (y + h);
    int x_start = (x < 0) ? 0 : x;
    int x_end = (x + w > win->alloc_w) ? win->alloc_w : (x + w);
    
    // Skip if completely out of bounds
    if (y_start >= y_end || x_start >= x_end) return;
    
    for (int row = y_start; row < y_end; ++row) {
        for (int col = x_start; col < x_end; ++col) {
            win->pixels[row * win->alloc_w + col] = color;
        }
    }
    compositor_mark_dirty();
}

// Font (same as in glass_renderer)
static const uint16_t s_font[91] = {
    0x0000,0x2252,0x6600,0x5F5F,0x27C2,0x4494,0x2AE4,0x4400,0x2492,0x4924,
    0x15A5,0x04E4,0x0012,0x01C0,0x0010,0x2248,0x76F7,0x2727,0x74F1,0x70E7,
    0x57C4,0x71C7,0x31E7,0x74A4,0x76E7,0x76C7,0x0440,0x0442,0x2492,0x0E0E,
    0x4924,0x6262,0x76B7,0x76F7,0x76E6,0x7627,0x66F6,0x71E1,0x71E4,0x7627,
    0x57F5,0x7227,0x2267,0x5AE5,0x2227,0x77D5,0x57B5,0x76F6,0x76E4,0x76F2,
    0x76EA,0x71C7,0x74A2,0x57F7,0x56A2,0x57B7,0x56A5,0x56A2,0x74A7,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
};

// Draw char in window buffer
static void draw_char_in_buf(uint32_t *pixels, int w, int h, int x, int y, 
                             char ch, uint32_t color) {
    if (!pixels || w <= 0 || h <= 0) return;
    if (ch < 32 || ch > 90) ch = 32;
    uint16_t bits = s_font[ch - 32];
    const int SCALE = 2; // scale font by 2x for larger, more readable text
    
    for (int row = 0; row < 5; ++row) {
        uint8_t rb = (uint8_t)((bits >> ((4 - row) * 3)) & 0x7);
        for (int c = 0; c < 3; ++c) {
            if (rb & (4 >> c)) {
                int px = x + c * SCALE;
                int py = y + row * SCALE;
                // draw SCALE x SCALE block
                for (int sy = 0; sy < SCALE; ++sy) {
                    int ry = py + sy;
                    if (ry < 0 || ry >= h) continue;
                    for (int sx = 0; sx < SCALE; ++sx) {
                        int rx = px + sx;
                        if (rx < 0 || rx >= w) continue;
                        int idx = ry * w + rx;
                        if (idx >= 0 && idx < w * h)
                            pixels[idx] = color;
                    }
                }
            }
        }
    }
}

// Write text to window
extern "C"
void wm_write(void *wv, int x, int y, const char *text, uint32_t color) {
    Window *win = (Window*)wv;
    if (!win || !win->pixels || !text || win->alloc_w <= 0 || win->alloc_h <= 0) return;
    
    // Clamp starting position to reasonable bounds
    if (x < -100 || x > win->alloc_w + 100) return;  // Way out of bounds
    if (y < -100 || y > win->alloc_h + 100) return;

    draw_string_fb_scaled(win->pixels, win->alloc_w, win->alloc_h, x, y, text, color, 1);
    compositor_mark_dirty();
}

// Get window by ID (for syscalls)
extern "C"
void* wm_get_window_by_id(int wid) {
    return (void*)get_window_by_id(wid);
}

// Register window and return ID
extern "C"
int wm_window_get_id(void *wv) {
    Window *win = (Window*)wv;
    if (!win) return 0;
    // Find existing ID
    for (int i = 0; i < MAX_WINDOW_IDS; ++i)
        if (s_window_ids[i] == wv) return i + 1;
    // Register new
    return register_window(wv);
}
