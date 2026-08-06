// kernel/ipc/capabilities.c
#include <stdint.h>
#include <stddef.h>

#define MAX_CAPS_PER_TASK 128
#define CAP_INVALID 0xFFFFFFFFFFFFFFFFULL

typedef enum {
    CAP_PORT,
    CAP_CHANNEL,
    CAP_SHM,
    CAP_DEVICE
} cap_type_t;

typedef struct capability {
    uint64_t id;
    cap_type_t type;
    uint64_t rights;
    void *object;
} capability_t;

typedef struct cap_table {
    capability_t caps[MAX_CAPS_PER_TASK];
} cap_table_t;

static uint64_t next_cap_id = 1;

cap_table_t *cap_table_create(void)
{
    extern void *slab_alloc(uint32_t);
    cap_table_t *tbl = (cap_table_t*)slab_alloc(sizeof(cap_table_t));
    for (int i = 0; i < MAX_CAPS_PER_TASK; ++i)
        tbl->caps[i].id = CAP_INVALID;
    return tbl;
}

uint64_t cap_create(cap_table_t *tbl, cap_type_t type, void *obj, uint64_t rights)
{
    for (int i = 0; i < MAX_CAPS_PER_TASK; ++i) {
        if (tbl->caps[i].id == CAP_INVALID) {
            tbl->caps[i].id = next_cap_id++;
            tbl->caps[i].type = type;
            tbl->caps[i].rights = rights;
            tbl->caps[i].object = obj;
            return tbl->caps[i].id;
        }
    }
    return CAP_INVALID;
}

capability_t *cap_lookup(cap_table_t *tbl, uint64_t id)
{
    for (int i = 0; i < MAX_CAPS_PER_TASK; ++i)
        if (tbl->caps[i].id == id)
            return &tbl->caps[i];
    return NULL;
}

int cap_check(capability_t *cap, uint64_t required)
{
    return cap && ((cap->rights & required) == required);
}
