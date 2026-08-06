// userland/compositor/scene_graph.cpp
// Scene graph minimalista: tick sense animació activa per ara.
// Les animacions es faran en fases posteriors.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern void wm_tick_animations(uint64_t now_ticks);
extern uint64_t scheduler_get_ticks(void);

void scene_graph_tick(void)
{
    uint64_t now = scheduler_get_ticks();
    wm_tick_animations(now);
}
