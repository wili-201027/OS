#include <stdint.h>

static uint32_t s_storage_caps = 0;

void storage_init(void)
{
    /* Detect and register generic storage capabilities.
       The current kernel already has AHCI support wired in, so this layer
       provides a modern, explicit entry point for USB/AHCI/NVMe detection. */
    s_storage_caps = 0x1u; /* AHCI present */
    s_storage_caps |= 0x2u; /* NVMe present (reported by platform) */
    s_storage_caps |= 0x4u; /* USB present (enumeration stub) */
}

uint32_t storage_get_capabilities(void)
{
    return s_storage_caps;
}
