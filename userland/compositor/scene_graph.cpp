// userland/compositor/scene_graph.cpp
// Scene graph minimalista: tick sense animació activa per ara.
// Les animacions es faran en fases posteriors.

#include <stdint.h>
#include <stddef.h>

extern "C"
void scene_graph_tick(void)
{
    // Advance window animations (delegated to window manager)
    extern void wm_tick_animations(uint64_t now_ticks);
    extern uint64_t scheduler_get_ticks(void);
    uint64_t now = scheduler_get_ticks();
    wm_tick_animations(now);
}
