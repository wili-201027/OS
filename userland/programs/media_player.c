// userland/programs/media_player.c
// Reproductor de audio y video MP3, WAV, MP4, AVI, etc.

#include <stdint.h>
#include <stdio.h>
#include "../libc/string.h"
#include "../libc/stdlib.h"

extern uint32_t  fb_get_width(void);
extern uint32_t  fb_get_height(void);
extern uint32_t *fb_get_addr(void);
extern void      sys_sleep_ms(uint32_t ms);
extern uint64_t  scheduler_get_ticks(void);

extern void *wm_create_window(int x, int y, int w, int h, const char *title);
extern void  wm_clear_window(void *win, uint32_t color);
extern void  wm_write(void *win, int x, int y, const char *text, uint32_t color);
extern void  wm_fill_rect(void *win, int x, int y, int w, int h, uint32_t color);

// Forward declarations for metadata readers
// Forward declare MediaPlayer type so prototypes can reference it
typedef struct MediaPlayer MediaPlayer;

// Forward declarations for metadata readers (use concrete type)
static void pm_read_mp3_metadata(const char *filename, MediaPlayer *player);
static void pm_read_wav_metadata(const char *filename, MediaPlayer *player);
static void pm_read_mp4_metadata(const char *filename, MediaPlayer *player);

// ─── Estados del reproductor ────────────────────────────────────────────────
typedef enum {
    MEDIA_STATE_STOPPED,
    MEDIA_STATE_PLAYING,
    MEDIA_STATE_PAUSED,
} MediaState;

// ─── Tipos de media ──────────────────────────────────────────────────────────
typedef enum {
    MEDIA_TYPE_UNKNOWN,
    MEDIA_TYPE_AUDIO_MP3,
    MEDIA_TYPE_AUDIO_WAV,
    MEDIA_TYPE_AUDIO_OGG,
    MEDIA_TYPE_VIDEO_MP4,
    MEDIA_TYPE_VIDEO_AVI,
    MEDIA_TYPE_VIDEO_MKV,
} MediaType;

// ─── Estructura del reproductor ──────────────────────────────────────────────
struct MediaPlayer {
    char       filename[256];
    MediaType  type;
    MediaState state;
    uint64_t   duration_ms;      // Duración total en ms
    uint64_t   current_pos_ms;   // Posición actual en ms
    uint32_t   volume;           // 0-100
    uint8_t    is_muted;
    uint32_t   bitrate;          // kbps
    char       audio_codec[32];  // "MP3", "AAC", etc.
    char       video_codec[32];  // "H.264", etc.
};

// ─── Detectar tipo de media ──────────────────────────────────────────────

MediaType detect_media_type(const char *filename)
{
    const char *dot = 0;
    for(int i = 0; filename[i]; i++) {
        if(filename[i] == '.') dot = &filename[i];
    }
    
    if(!dot) return MEDIA_TYPE_UNKNOWN;
    
    const char *ext = dot + 1;
    
    if(strcmp(ext, "mp3") == 0) return MEDIA_TYPE_AUDIO_MP3;
    if(strcmp(ext, "wav") == 0) return MEDIA_TYPE_AUDIO_WAV;
    if(strcmp(ext, "ogg") == 0) return MEDIA_TYPE_AUDIO_OGG;
    if(strcmp(ext, "mp4") == 0) return MEDIA_TYPE_VIDEO_MP4;
    if(strcmp(ext, "avi") == 0) return MEDIA_TYPE_VIDEO_AVI;
    if(strcmp(ext, "mkv") == 0) return MEDIA_TYPE_VIDEO_MKV;
    
    return MEDIA_TYPE_UNKNOWN;
}

// ─── Crear reproductor ─────────────────────────────────────────────────────

MediaPlayer* player_create(const char *filename)
{
    MediaPlayer *player = (MediaPlayer*)malloc(sizeof(MediaPlayer));
    if(!player) return 0;
    
    strcpy(player->filename, filename);
    player->type = detect_media_type(filename);
    player->state = MEDIA_STATE_STOPPED;
    player->duration_ms = 0;
    player->current_pos_ms = 0;
    player->volume = 70;
    player->is_muted = 0;
    player->bitrate = 0;
    
    strcpy(player->audio_codec, "Unknown");
    strcpy(player->video_codec, "Unknown");
    
    // Leer metadata del archivo según tipo
    switch(player->type) {
        case MEDIA_TYPE_AUDIO_MP3:
            pm_read_mp3_metadata(filename, player);
            break;
        case MEDIA_TYPE_AUDIO_WAV:
            pm_read_wav_metadata(filename, player);
            break;
        case MEDIA_TYPE_VIDEO_MP4:
            pm_read_mp4_metadata(filename, player);
            break;
        default:
            strcpy(player->audio_codec, "Unknown");
            player->duration_ms = 0;
    }
    
    return player;
}

// Leer metadata ID3v2 de MP3
static void pm_read_mp3_metadata(const char *filename, MediaPlayer *player)
{
    extern long syscall_read(int fd, void *buf, unsigned long count);
    extern int syscall_open(const char *fname, int flags);
    extern int syscall_close(int fd);
    
    int fd = syscall_open(filename, 0);  // O_RDONLY
    if(fd < 0) return;
    
    // Leer primeros 3 bytes (ID3 signature)
    uint8_t header[10];
    if(syscall_read(fd, header, 10) != 10) {
        syscall_close(fd);
        return;
    }
    
    // Validar ID3v2 signature
    if(header[0] != 'I' || header[1] != 'D' || header[2] != '3') {
        // No tiene ID3v2, buscar info en frames MP3
        syscall_close(fd);
        strcpy(player->audio_codec, "MP3");
        player->bitrate = 128;  // Asumir 128kbps
        player->duration_ms = 180000;  // 3 minutos por defecto
        return;
    }
    
    // Calcular tamaño ID3v2 (synchsafe int en bytes 6-9)
    uint32_t id3_size = ((header[6] & 0x7F) << 21) | 
                        ((header[7] & 0x7F) << 14) |
                        ((header[8] & 0x7F) << 7) |
                        (header[9] & 0x7F);
    
    // Saltarpor ID3
    uint8_t skip_buf[512];
    uint32_t remaining = id3_size;
    while(remaining > 0) {
        uint32_t to_read = remaining > sizeof(skip_buf) ? sizeof(skip_buf) : remaining;
        if(syscall_read(fd, skip_buf, to_read) != (long)to_read) break;
        remaining -= to_read;
    }
    
    // Leer primer frame MP3 (donde están bitrate y sample rate)
    // Frame header: 11 bits sincronización (0xFFF), luego datos
    uint8_t frame[4];
    if(syscall_read(fd, frame, 4) == 4) {
        // Frame format (FFFB AAAC CCDD EEFF)
        // AA = MPEG info
        // CC = bitrate index
        // DD = sample rate index
        
        int bitrate_table[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1};
        int samplerate_table[] = {44100, 48000, 32000, 0};
        
        int bitrate_idx = (frame[2] >> 4) & 0xF;
        int samplerate_idx = (frame[2] >> 2) & 0x3;
        
        player->bitrate = bitrate_table[bitrate_idx];
        int sample_rate = samplerate_table[samplerate_idx];
        
        // Estimación simple de duración (archivo completo en MP3 sin seek table)
        // En realidad: necesitaría buscar último frame o usar Xing tag
        player->duration_ms = 180000;  // 3 minutos por defecto
        
        strcpy(player->audio_codec, "MP3");
    }
    
    syscall_close(fd);
}

// Leer metadata de WAV
static void pm_read_wav_metadata(const char *filename, MediaPlayer *player)
{
    extern long syscall_read(int fd, void *buf, unsigned long count);
    extern int syscall_open(const char *fname, int flags);
    extern int syscall_close(int fd);
    
    int fd = syscall_open(filename, 0);  // O_RDONLY
    if(fd < 0) return;
    
    // WAV header: RIFF format
    struct {
        char     riff_id[4];      // "RIFF"
        uint32_t file_size;
        char     wave_id[4];      // "WAVE"
        char     fmt_id[4];       // "fmt "
        uint32_t fmt_size;
        uint16_t format_code;     // 1 = PCM
        uint16_t num_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
    } wav_header;
    
    if(syscall_read(fd, &wav_header, sizeof(wav_header)) != sizeof(wav_header)) {
        syscall_close(fd);
        return;
    }
    
    // Validar firma
    if(wav_header.riff_id[0] != 'R' || wav_header.wave_id[0] != 'W') {
        syscall_close(fd);
        return;
    }
    
    // Calcular duración
    // file_size - header = datos de audio
    // duración = tamaño datos / byte_rate (bytes por segundo)
    uint32_t audio_data_size = wav_header.file_size - 36;
    if(wav_header.byte_rate > 0) {
        player->duration_ms = (audio_data_size * 1000) / wav_header.byte_rate;
    }
    
    player->bitrate = wav_header.byte_rate * 8 / 1000;  // bytes/sec -> kbps
    
    sprintf(player->audio_codec, "WAV PCM %dch %dHz", 
            wav_header.num_channels, wav_header.sample_rate);
    
    syscall_close(fd);
}

// Leer metadata de MP4
static void pm_read_mp4_metadata(const char *filename, MediaPlayer *player)
{
    extern long syscall_read(int fd, void *buf, unsigned long count);
    extern int syscall_open(const char *fname, int flags);
    extern int syscall_close(int fd);
    
    int fd = syscall_open(filename, 0);  // O_RDONLY
    if(fd < 0) return;
    
    // MP4 es formato complejo con atoms
    // Búsqueda simple: encontrar "mvhd" (movie header)
    uint8_t buffer[512];
    long bytes_read;
    
    while((bytes_read = syscall_read(fd, buffer, sizeof(buffer))) > 0) {
        // Buscar "mvhd" en buffer
        for(int i = 0; i < bytes_read - 20; i++) {
            if(buffer[i+4] == 'm' && buffer[i+5] == 'v' && 
               buffer[i+6] == 'h' && buffer[i+7] == 'd') {
                // Encontrado mvhd, leer duración
                // En bytes [i+16-19]: time scale (big-endian uint32)
                // En bytes [i+20-27]: duration (big-endian uint64)
                
                uint32_t time_scale = ((uint32_t)buffer[i+16] << 24) |
                                     ((uint32_t)buffer[i+17] << 16) |
                                     ((uint32_t)buffer[i+18] << 8) |
                                     buffer[i+19];
                
                uint64_t duration_ticks = ((uint64_t)buffer[i+20] << 56) |
                                         ((uint64_t)buffer[i+21] << 48) |
                                         ((uint64_t)buffer[i+22] << 40) |
                                         ((uint64_t)buffer[i+23] << 32) |
                                         ((uint64_t)buffer[i+24] << 24) |
                                         ((uint64_t)buffer[i+25] << 16) |
                                         ((uint64_t)buffer[i+26] << 8) |
                                         buffer[i+27];
                
                if(time_scale > 0) {
                    player->duration_ms = (duration_ticks * 1000) / time_scale;
                    player->bitrate = 1000;  // Estimado
                    strcpy(player->video_codec, "H.264");
                    strcpy(player->audio_codec, "AAC");
                }
                
                syscall_close(fd);
                return;
            }
        }
    }
    
    syscall_close(fd);
}

void player_destroy(MediaPlayer *player)
{
    if(player) free(player);
}

// ─── Controles de reproducción ────────────────────────────────────────────

void player_play(MediaPlayer *player)
{
    if(player && player->state == MEDIA_STATE_STOPPED) {
        player->state = MEDIA_STATE_PLAYING;
        
        // Iniciar playback en kernel/drivers/audio
        // Syscall: sys_audio_play(player->filename, player->volume);
        // Esto requeriría:
        // - Driver de audio en hardware (se implementaría en kernel)
        // - DMA para enviar datos a codec de audio
        // - Interrupt handlers para sincronización
        // - Estado de playback compartido entre kernel y userland
        
        // Para ahora: solo actualizar estado
        player->current_pos_ms = 0;
    }
}

void player_pause(MediaPlayer *player)
{
    if(player && player->state == MEDIA_STATE_PLAYING) {
        player->state = MEDIA_STATE_PAUSED;
        // Syscall: sys_audio_pause();
    }
}

void player_resume(MediaPlayer *player)
{
    if(player && player->state == MEDIA_STATE_PAUSED) {
        player->state = MEDIA_STATE_PLAYING;
        // Syscall: sys_audio_resume();
    }
}

void player_stop(MediaPlayer *player)
{
    if(player) {
        player->state = MEDIA_STATE_STOPPED;
        player->current_pos_ms = 0;
        // Syscall: sys_audio_stop();
    }
}

void player_seek(MediaPlayer *player, uint64_t position_ms)
{
    if(player && position_ms <= player->duration_ms) {
        player->current_pos_ms = position_ms;
        
        // Seek en el archivo
        // Syscall: sys_audio_seek(position_ms);
        // Esto requeriría:
        // - Búsqueda en frames del archivo de audio
        // - Recalcular posición en bytes desde ms
        // - Sincronización con playback
    }
}

void player_set_volume(MediaPlayer *player, uint32_t volume)
{
    if(player && volume <= 100) {
        player->volume = volume;
        
        // Cambiar volumen en hardware audio
        // Syscall: sys_audio_set_volume(volume);
        // Esto requeriría:
        // - Control de ganancia en codec de audio
        // - Master volume en hardware ALSA/OSS
        // - Actualización en tiempo real sin interrumpir playback
    }
}

// ─── Formateo de tiempo ──────────────────────────────────────────────────

static void format_time(char *buf, uint64_t ms)
{
    uint32_t secs = ms / 1000;
    uint32_t mins = secs / 60;
    uint32_t hours = mins / 60;
    
    secs %= 60;
    mins %= 60;
    
    if(hours > 0) {
        sprintf(buf, "%u:%02u:%02u", hours, mins, secs);
    } else {
        sprintf(buf, "%u:%02u", mins, secs);
    }
}

// ─── Dibujo del reproductor ──────────────────────────────────────────────────

void player_draw(MediaPlayer *player, uint32_t *framebuffer, uint32_t fb_width,
                 int wx, int wy, int ww, int wh)
{
    if(!player || !framebuffer) return;
    
    // Fondo oscuro
    for(int row = wy; row < wy + wh && row < (int)fb_get_height(); row++) {
        for(int col = wx; col < wx + ww && col < (int)fb_width; col++) {
            if(row >= 0 && col >= 0) {
                framebuffer[row * fb_width + col] = 0xFF1A1A1A;
            }
        }
    }
    
    extern void draw_string_fb(uint32_t*, uint32_t, uint32_t, int, int, const char*, uint32_t);
    
    // Mostrar información
    int y = wy + 20;
    draw_string_fb(framebuffer, fb_width, fb_get_height(), wx + 10, y, player->filename, 0xFFFFFFFF);
    
    // Mostrar estado
    y += 20;
    const char *state_str = (player->state == MEDIA_STATE_PLAYING) ? "Playing" :
                            (player->state == MEDIA_STATE_PAUSED) ? "Paused" : "Stopped";
    draw_string_fb(framebuffer, fb_width, fb_get_height(), wx + 10, y, state_str, 0xFF00FF00);
    
    // Mostrar duración y posición
    y += 20;
    char time_str[64];
    char pos_str[32], dur_str[32];
    format_time(pos_str, player->current_pos_ms);
    format_time(dur_str, player->duration_ms);
    sprintf(time_str, "%s / %s", pos_str, dur_str);
    draw_string_fb(framebuffer, fb_width, fb_get_height(), wx + 10, y, time_str, 0xFFFFFF00);
    
    // Mostrar bitrate y codecs
    y += 20;
    char codec_str[128];
    sprintf(codec_str, "Audio: %s @ %u kbps | Video: %s", 
            player->audio_codec, player->bitrate, player->video_codec);
    draw_string_fb(framebuffer, fb_width, fb_get_height(), wx + 10, y, codec_str, 0xFF888888);
    
    // Mostrar controles
    y += 40;
    draw_string_fb(framebuffer, fb_width, fb_get_height(), wx + 10, y, 
                   "[SPC] Play/Pause  [S] Stop  [+/-] Volume  [<>] Seek", 0xFF00CCFF);
}

// ─── Función principal ────────────────────────────────────────────────────

int media_player_main(const char *filename)
{
    MediaPlayer *player = player_create(filename);
    if(!player) return -1;
    
    void *window = wm_create_window(100, 100, 640, 480, filename);
    if(!window) {
        player_destroy(player);
        return -1;
    }
    
    player_play(player);
    
    // Loop principal
    while(1) {
        sys_sleep_ms(100);
        
        // Actualizar posición si está reproduciendo
        if(player->state == MEDIA_STATE_PLAYING) {
            player->current_pos_ms += 100;
            if(player->current_pos_ms >= player->duration_ms) {
                player_stop(player);
            }
        }
        
        // Procesar entrada y actualizar pantalla
    }
    
    player_destroy(player);
    return 0;
}
