// userland/compositor/glass_renderer.cpp
// Pipeline glassmorphism:
//   1. Wallpaper degradado (blau fosc → lila)
//   2. Per-finestra: box-blur del fons + tint glass + contingut + decoració
//   3. Cursor
// Corre en ring-0, sense syscalls.

#include <stdint.h>
#include <stddef.h>

extern "C" {
    uint32_t  fb_get_width(void);
    uint32_t  fb_get_height(void);
    uint32_t *fb_get_addr(void);
    void      fb_copy_pixels(const uint32_t *src, uint32_t *dst, uint32_t pixel_count);
    void     *malloc(uint32_t);
    void      free(void*);
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static inline uint8_t clamp8(int v){ return (uint8_t)(v<0?0:v>255?255:v); }

static inline uint32_t blend(uint32_t dst, uint32_t src, uint8_t a){
    uint32_t ia=256-a;
    uint8_t r=clamp8((int)(((dst>>16)&0xFF)*ia+((src>>16)&0xFF)*a)>>8);
    uint8_t g=clamp8((int)(((dst>> 8)&0xFF)*ia+((src>> 8)&0xFF)*a)>>8);
    uint8_t b=clamp8((int)(((dst    )&0xFF)*ia+((src    )&0xFF)*a)>>8);
    return 0xFF000000u|(uint32_t)(r<<16)|(uint32_t)(g<<8)|b;
}
static inline uint32_t rgb(uint8_t r,uint8_t g,uint8_t b){ return 0xFF000000u|(uint32_t)(r<<16)|(uint32_t)(g<<8)|b; }

// ── Box blur sliding window ───────────────────────────────────────────────────
static uint32_t *s_blur_tmp = nullptr;
static uint32_t  s_blur_sz  = 0;

// Double buffering
static uint32_t *s_back_buffer = nullptr;
static uint32_t  s_back_w = 0, s_back_h = 0;

static bool ensure_back_buffer(uint32_t w, uint32_t h) {
    if (s_back_buffer && s_back_w == w && s_back_h == h) {
        return true;  // Already allocated correctly
    }
    
    // Free old buffer if resizing
    if (s_back_buffer) {
        free(s_back_buffer);
        s_back_buffer = nullptr;
    }
    
    // Allocate new buffer
    uint64_t pixel_count = (uint64_t)w * (uint64_t)h;
    uint64_t size64 = pixel_count * (uint64_t)sizeof(uint32_t);
    if (size64 > 0xFFFFFFFFu) {
        s_back_w = 0;
        s_back_h = 0;
        return false;
    }
    uint32_t size = (uint32_t)size64;
    s_back_buffer = (uint32_t*)malloc(size);
    
    if (s_back_buffer) {
        s_back_w = w;
        s_back_h = h;
        return true;
    }
    
    s_back_w = 0;
    s_back_h = 0;
    return false;
}

static void ensure_blur(uint32_t n){
    if(n>s_blur_sz){ if(s_blur_tmp) free(s_blur_tmp); s_blur_tmp=(uint32_t*)malloc(n * sizeof(uint32_t)); s_blur_sz=n; }
}

static void box_blur_region(uint32_t*fb, uint32_t fbw,
                             int x0,int y0,int rw,int rh, int r)
{
    if(rw<=0||rh<=0||r<=0) return;

    /* Clamp region to framebuffer bounds to avoid out-of-bounds access */
    int x1 = x0;
    int y1 = y0;
    int x2 = x0 + rw; // exclusive
    int y2 = y0 + rh; // exclusive
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > (int)fbw) x2 = (int)fbw;
    /* fb height is not passed; query it */
    uint32_t fb_h = fb_get_height();
    if (y2 > (int)fb_h) y2 = (int)fb_h;
    int nrw = x2 - x1;
    int nrh = y2 - y1;
    if (nrw <= 0 || nrh <= 0) return;

    ensure_blur((uint32_t)(nrw * nrh));
    if(!s_blur_tmp) return;

    // Horizontal pass: FB → tmp (only for clamped region)
    for(int row=0; row<nrh; ++row){
        int fy = y1 + row;
        uint32_t *src = fb + fy * fbw + x1;
        uint32_t *dst = s_blur_tmp + row * nrw;
        int rA = 0, gA = 0, bA = 0, cnt = r + 1;
        for(int k=0; k<=r; ++k){ int sx = (k < nrw) ? k : nrw - 1; uint32_t c = src[sx]; rA += (c>>16)&0xFF; gA += (c>>8)&0xFF; bA += c&0xFF; }
        for(int col=0; col<nrw; ++col){
            dst[col] = rgb(clamp8(rA/cnt), clamp8(gA/cnt), clamp8(bA/cnt));
            int add = col + r + 1, rem = col - r;
            if (add < nrw){ uint32_t c = src[add]; rA += (c>>16)&0xFF; gA += (c>>8)&0xFF; bA += c&0xFF; ++cnt; }
            if (rem >= 0){ uint32_t c = src[rem]; rA -= (c>>16)&0xFF; gA -= (c>>8)&0xFF; bA -= c&0xFF; --cnt; }
        }
    }

    // Vertical pass: tmp → FB
    for(int col=0; col<nrw; ++col){
        int rA = 0, gA = 0, bA = 0, cnt = r + 1;
        for(int k=0; k<=r; ++k){ int sy = (k < nrh) ? k : nrh - 1; uint32_t c = s_blur_tmp[sy * nrw + col]; rA += (c>>16)&0xFF; gA += (c>>8)&0xFF; bA += c&0xFF; }
        for(int row=0; row<nrh; ++row){
            fb[(y1 + row) * fbw + (x1 + col)] = rgb(clamp8(rA/cnt), clamp8(gA/cnt), clamp8(bA/cnt));
            int add = row + r + 1, rem = row - r;
            if (add < nrh){ uint32_t c = s_blur_tmp[add * nrw + col]; rA += (c>>16)&0xFF; gA += (c>>8)&0xFF; bA += c&0xFF; ++cnt; }
            if (rem >= 0){ uint32_t c = s_blur_tmp[rem * nrw + col]; rA -= (c>>16)&0xFF; gA -= (c>>8)&0xFF; bA -= c&0xFF; --cnt; }
        }
    }
}

// ── Font 8×16 ─────────────────────────────────────────────────────────────────
static const uint8_t s_font8x16[] = {
    // 32 ' '
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    // 33 '!'
    0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00,
    // 34 '"'
    0x24,0x24,0x24,0x00,0x00,0x00,0x00,0x00,
    // 35 '#'
    0x24,0x24,0x7E,0x24,0x7E,0x24,0x24,0x00,
    // 36 '$'
    0x10,0x3C,0x50,0x38,0x14,0x7C,0x10,0x00,
    // 37 '%'
    0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00,
    // 38 '&'
    0x38,0x44,0x48,0x30,0x4A,0x44,0x3A,0x00,
    // 39 '\''
    0x18,0x18,0x10,0x00,0x00,0x00,0x00,0x00,
    // 40 '('
    0x0C,0x10,0x20,0x20,0x20,0x10,0x0C,0x00,
    // 41 ')'
    0x30,0x08,0x04,0x04,0x04,0x08,0x30,0x00,
    // 42 '*'
    0x00,0x24,0x18,0x7E,0x18,0x24,0x00,0x00,
    // 43 '+'
    0x00,0x10,0x10,0x7C,0x10,0x10,0x00,0x00,
    // 44 ','
    0x00,0x00,0x00,0x00,0x18,0x18,0x10,0x00,
    // 45 '-'
    0x00,0x00,0x00,0x7C,0x00,0x00,0x00,0x00,
    // 46 '.'
    0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,
    // 47 '/'
    0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x00,
    // 48 '0'
    0x3C,0x42,0x46,0x4A,0x52,0x62,0x3C,0x00,
    // 49 '1'
    0x10,0x30,0x10,0x10,0x10,0x10,0x7C,0x00,
    // 50 '2'
    0x3C,0x42,0x02,0x1C,0x20,0x40,0x7E,0x00,
    // 51 '3'
    0x3C,0x42,0x02,0x1C,0x02,0x42,0x3C,0x00,
    // 52 '4'
    0x04,0x0C,0x14,0x24,0x7E,0x04,0x04,0x00,
    // 53 '5'
    0x7E,0x40,0x40,0x7C,0x02,0x42,0x3C,0x00,
    // 54 '6'
    0x1C,0x20,0x40,0x7C,0x42,0x42,0x3C,0x00,
    // 55 '7'
    0x7E,0x02,0x04,0x08,0x10,0x10,0x10,0x00,
    // 56 '8'
    0x3C,0x42,0x42,0x3C,0x42,0x42,0x3C,0x00,
    // 57 '9'
    0x3C,0x42,0x42,0x3E,0x02,0x04,0x38,0x00,
    // 58 ':'
    0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00,
    // 59 ';'
    0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x10,
    // 60 '<'
    0x04,0x08,0x10,0x20,0x10,0x08,0x04,0x00,
    // 61 '='
    0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00,
    // 62 '>'
    0x40,0x20,0x10,0x08,0x10,0x20,0x40,0x00,
    // 63 '?'
    0x3C,0x42,0x02,0x04,0x08,0x00,0x08,0x00,
    // 64 '@'
    0x3C,0x42,0x5A,0x5A,0x5E,0x40,0x3C,0x00,
    // 65 'A'
    0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x00,
    // 66 'B'
    0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C,0x00,
    // 67 'C'
    0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00,
    // 68 'D'
    0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00,
    // 69 'E'
    0x7E,0x40,0x40,0x7C,0x40,0x40,0x7E,0x00,
    // 70 'F'
    0x7E,0x40,0x40,0x7C,0x40,0x40,0x40,0x00,
    // 71 'G'
    0x3C,0x42,0x40,0x4E,0x42,0x42,0x3C,0x00,
    // 72 'H'
    0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00,
    // 73 'I'
    0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,
    // 74 'J'
    0x1E,0x04,0x04,0x04,0x04,0x44,0x38,0x00,
    // 75 'K'
    0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00,
    // 76 'L'
    0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00,
    // 77 'M'
    0x42,0x66,0x5A,0x5A,0x42,0x42,0x42,0x00,
    // 78 'N'
    0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00,
    // 79 'O'
    0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,
    // 80 'P'
    0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00,
    // 81 'Q'
    0x3C,0x42,0x42,0x42,0x4A,0x3C,0x0A,0x00,
    // 82 'R'
    0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00,
    // 83 'S'
    0x3C,0x42,0x40,0x3C,0x02,0x42,0x3C,0x00,
    // 84 'T'
    0x7E,0x10,0x10,0x10,0x10,0x10,0x10,0x00,
    // 85 'U'
    0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,
    // 86 'V'
    0x42,0x42,0x42,0x42,0x42,0x24,0x18,0x00,
    // 87 'W'
    0x42,0x42,0x42,0x5A,0x5A,0x5A,0x24,0x00,
    // 88 'X'
    0x42,0x42,0x24,0x18,0x24,0x42,0x42,0x00,
    // 89 'Y'
    0x42,0x42,0x24,0x18,0x10,0x10,0x10,0x00,
    // 90 'Z'
    0x7E,0x02,0x04,0x08,0x10,0x20,0x7E,0x00,
};

static void draw_char_fb(uint32_t*fb,uint32_t fbw,uint32_t fbh,
                          int px,int py,char ch,uint32_t col,int scale=1)
{
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch < 32 || ch > 90) ch = 32;
    const uint8_t *glyph = s_font8x16 + (size_t)(ch - 32) * 8;
    for (int row = 0; row < 8; ++row) {
        uint8_t bits = glyph[row];
        for (int dy = 0; dy < 2; ++dy) {
            int y = py + (row * 2 + dy) * scale;
            if (y < 0 || (uint32_t)y >= fbh) continue;
            for (int c = 0; c < 8; ++c) {
                if (!(bits & (0x80 >> c))) continue;
                for (int sy = 0; sy < scale; ++sy) {
                    for (int sx = 0; sx < scale; ++sx) {
                        int x = px + c * scale + sx;
                        if (x < 0 || (uint32_t)x >= fbw) continue;
                        fb[y * fbw + x] = col;
                    }
                }
            }
        }
    }
}

extern "C"
void draw_string_fb(uint32_t*fb,uint32_t fbw,uint32_t fbh,
                    int x,int y,const char*s,uint32_t col)
{
    while(s&&*s){ draw_char_fb(fb,fbw,fbh,x,y,*s,col,1); x += 9; ++s; }
}

extern "C" void draw_string_fb_scaled(uint32_t*fb,uint32_t fbw,uint32_t fbh,
                                          int x,int y,const char*s,uint32_t col,int scale)
{
    while(s&&*s){ draw_char_fb(fb,fbw,fbh,x,y,*s,col,scale); x += 9 * scale; ++s; }
}

// ── Wallpaper ─────────────────────────────────────────────────────────────────
extern "C"
void render_wallpaper(uint32_t*fb,uint32_t w,uint32_t h)
{
    for(uint32_t y=0;y<h;++y){
        uint32_t t=(y*255)/(h>1?h-1:1);
        uint8_t r=(uint8_t)(0x01 + (0x0A * t / 255));
        uint8_t g=(uint8_t)(0x08 + (0x18 * t / 255));
        uint8_t b=(uint8_t)(0x18 + (0x50 * t / 255));
        uint32_t c=rgb(r,g,b);
        uint32_t*row=fb+y*w;
        for(uint32_t x=0;x<w;++x) row[x]=c;
    }

    // Subtle glass highlights and depth lines
    for(uint32_t y=0;y<h;++y){
        uint32_t x=(y*37 + (y*y)/9) % w;
        if(((y + x) & 0x3F) == 0) fb[y*w+x]=rgb(12, 70, 120);
    }

    for(int i=0;i<120;++i){
        uint32_t x=(uint32_t)((i*2654435761u+i*i)>>1)%w;
        uint32_t y=(uint32_t)((i*1234567891u+i*3)>>2)%h;
        if((i & 3) == 0) fb[y*w+x]=rgb(180,220,255);
    }

    const char* logo = "GPT-OS";
    int lx = (int)w/2 - 28;
    draw_string_fb_scaled(fb,w,h, lx, 8, logo, 0xFFEAF6FF, 2);
}

// ── API cridada per window_manager ───────────────────────────────────────────
extern "C"
void wm_render_all(uint32_t*fb,uint32_t w,uint32_t h,
                   void(*blur_fn)(uint32_t*,uint32_t,int,int,int,int,int));

// ── render_frame ─────────────────────────────────────────────────────────────
extern "C"
void render_frame(void)
{
    uint32_t  w = fb_get_width();
    uint32_t  h = fb_get_height();
    
    if (w == 0 || h == 0) return;
    
    // Ensure back buffer exists
    if (!ensure_back_buffer(w, h)) return;
    
    // Render everything to back buffer
    render_wallpaper(s_back_buffer, w, h);
    wm_render_all(s_back_buffer, w, h, box_blur_region);
}

// ── Back buffer management ──────────────────────────────────────────────────────
extern "C"
uint32_t *get_back_buffer(uint32_t w, uint32_t h)
{
    if (!ensure_back_buffer(w, h)) return nullptr;
    return s_back_buffer;
}

extern "C"
void flip_frame(uint32_t w, uint32_t h)
{
    uint32_t *fb = fb_get_addr();
    if (!fb || !w || !h || !s_back_buffer) return;
    
    // Fast copy to visible framebuffer using a hardware-friendly path.
    uint32_t total = w * h;
    fb_copy_pixels(s_back_buffer, fb, total);
}
