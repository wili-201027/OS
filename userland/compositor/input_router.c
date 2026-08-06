// userland/compositor/input_router.cpp
// Llegeix scancodes PS/2 i moviments de ratolí des del driver del kernel.
// En ring-0 podem cridar directament les funcions del driver.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern uint8_t ps2_read_scancode_nowait(void);
extern uint8_t ps2_read_mouse_nowait(void);

extern void wm_handle_mouse(int ax,int ay,bool btn,bool pressed,bool released);
extern void wm_handle_key(uint8_t sc,bool pressed);
extern void wm_close_focused_window(void);
extern void wm_cycle_focus(int direction);
extern void wm_toggle_menu(void);
extern void wm_move_focused_window(int dx,int dy);

extern uint32_t fb_get_width(void);
extern uint32_t fb_get_height(void);

static int    s_ax=400, s_ay=300;
static bool   s_btn=false;
static uint8_t s_mpkt[3];
static int    s_mpkt_idx=0;

// Key event tracking
#define KEY_BUFFER_SIZE 32
static uint8_t s_key_buf[KEY_BUFFER_SIZE];
static int s_key_head = 0;
static int s_key_tail = 0;

// Modifier key tracking (for keyboard shortcuts)
static bool s_alt_pressed = false;
static bool s_ctrl_pressed = false;
static bool s_shift_pressed = false;

static void push_key(uint8_t sc) {
    int next = (s_key_head + 1) % KEY_BUFFER_SIZE;
    if (next != s_key_tail) {
        s_key_buf[s_key_head] = sc;
        s_key_head = next;
    }
}

static void process_kb(uint8_t byte){
    bool pressed = (byte & 0x80) == 0;
    uint8_t sc = byte & 0x7F;
    
    // Track modifier keys
    if (sc == 0x38) {  // Left Alt
        s_alt_pressed = pressed;
    } else if (sc == 0x1D) {  // Left Ctrl
        s_ctrl_pressed = pressed;
    } else if (sc == 0x2A || sc == 0x36) {  // Left/Right Shift
        s_shift_pressed = pressed;
    }
    
    // Handle Alt+F4 (F4 is scancode 0x3E)
    if (pressed && s_alt_pressed && sc == 0x3E) {
        wm_close_focused_window();
        return;
    }

    if (pressed && s_alt_pressed && sc == 0x0F) {
        wm_cycle_focus(1);
        return;
    }

    if (pressed && s_alt_pressed && s_shift_pressed && sc == 0x0F) {
        wm_cycle_focus(-1);
        return;
    }

    if (pressed && (sc == 0x5B || sc == 0x5C)) {
        wm_toggle_menu();
        return;
    }

    if (pressed && sc == 0x4B) {
        wm_move_focused_window(-16, 0);
        return;
    }
    if (pressed && sc == 0x4D) {
        wm_move_focused_window(16, 0);
        return;
    }
    if (pressed && sc == 0x48) {
        wm_move_focused_window(0, -16);
        return;
    }
    if (pressed && sc == 0x50) {
        wm_move_focused_window(0, 16);
        return;
    }
    
    if (pressed) push_key(sc);
    wm_handle_key(sc, pressed);
}

static void process_mouse(uint8_t byte){
    s_mpkt[s_mpkt_idx++]=byte;
    if(s_mpkt_idx<3) return;
    s_mpkt_idx=0;

    uint8_t flags=s_mpkt[0];
    if(!(flags&0x08)) return;        // bit 3 sempre 1
    if(flags&0xC0)    return;        // overflow

    int8_t dx=(int8_t)s_mpkt[1];
    int8_t dy=(int8_t)s_mpkt[2];
    bool nb=(flags&0x01)!=0;
    bool pressed= nb&&!s_btn;
    bool released=!nb&&s_btn;

    s_ax += dx;
    s_ay -= dy; // PS/2 Y delta is positive when moving up, so invert for screen coordinates
    uint32_t W=fb_get_width(), H=fb_get_height();
    if(W>0&&H>0){
        if(s_ax<0) s_ax=0;
        if(s_ay<0) s_ay=0;
        if((uint32_t)s_ax>=W) s_ax=(int)W-1;
        if((uint32_t)s_ay>=H) s_ay=(int)H-1;
    }
    s_btn=nb;
    wm_handle_mouse(s_ax,s_ay,nb,pressed,released);
}


void input_router_poll(void){
    // Keyboard: llegir fins que no hi hagi dades
    for(int i=0;i<16;++i){
        uint8_t sc=ps2_read_scancode_nowait();
        if(!sc) break;
        process_kb(sc);
    }
    // Mouse: llegir fins que no hi hagi dades
    for(int i=0;i<32;++i){
        uint8_t mb=ps2_read_mouse_nowait();
        if(mb==0xFF) break;
        process_mouse(mb);
    }
}


void input_get_cursor(int*x,int*y){ if(x)*x=s_ax; if(y)*y=s_ay; }

// Get next keystroke (returns 0 if no key available)

uint8_t input_get_key(void) {
    if (s_key_head == s_key_tail) return 0;
    uint8_t sc = s_key_buf[s_key_tail];
    s_key_tail = (s_key_tail + 1) % KEY_BUFFER_SIZE;
    return sc;
}

// Get current mouse position

void input_get_mouse(int *x, int *y, bool *btn) {
    if (x) *x = s_ax;
    if (y) *y = s_ay;
    if (btn) *btn = s_btn;
}
