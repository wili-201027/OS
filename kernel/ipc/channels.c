// kernel/ipc/channels.c
#include <stdint.h>
#include <stddef.h>

#define CHANNEL_BUFFER_SIZE 4096

typedef struct channel {
    uint8_t buffer[CHANNEL_BUFFER_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
} channel_t;

extern void *slab_alloc(uint32_t);

channel_t *channel_create(void)
{
    channel_t *ch = (channel_t*)slab_alloc(sizeof(channel_t));
    ch->read_pos = 0;
    ch->write_pos = 0;
    return ch;
}

uint32_t channel_write(channel_t *ch, const void *data, uint32_t len)
{
    uint32_t written = 0;
    const uint8_t *src = (const uint8_t*)data;

    while (written < len) {
        uint32_t next = (ch->write_pos + 1) % CHANNEL_BUFFER_SIZE;
        if (next == ch->read_pos)
            break;

        ch->buffer[ch->write_pos] = src[written++];
        ch->write_pos = next;
    }
    return written;
}

uint32_t channel_read(channel_t *ch, void *data, uint32_t len)
{
    uint8_t *dst = (uint8_t*)data;
    uint32_t read = 0;

    while (read < len && ch->read_pos != ch->write_pos) {
        dst[read++] = ch->buffer[ch->read_pos];
        ch->read_pos = (ch->read_pos + 1) % CHANNEL_BUFFER_SIZE;
    }
    return read;
}
