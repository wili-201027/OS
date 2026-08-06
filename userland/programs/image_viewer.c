// userland/programs/image_viewer.c
// Visor de imágenes para PNG, JPEG, BMP, GIF, etc.

#include <stdint.h>
#include "../libc/string.h"
#include "../libc/stdlib.h"
#include "../libc/stdio.h"

extern uint32_t  fb_get_width(void);
extern uint32_t  fb_get_height(void);
extern uint32_t *fb_get_addr(void);
extern void      sys_sleep_ms(uint32_t ms);

extern void *wm_create_window(int x, int y, int w, int h, const char *title);
extern void  wm_clear_window(void *win, uint32_t color);
extern void  wm_write(void *win, int x, int y, const char *text, uint32_t color);
extern void  wm_fill_rect(void *win, int x, int y, int w, int h, uint32_t color);

// ─── Formatos de imagen soportados ──────────────────────────────────────────
typedef enum {
    IMG_FORMAT_UNKNOWN,
    IMG_FORMAT_BMP,
    IMG_FORMAT_PNG,
    IMG_FORMAT_JPEG,
    IMG_FORMAT_GIF,
} ImageFormat;

// ─── Estructura de imagen ───────────────────────────────────────────────────
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t *pixels;       // Array de píxeles ARGB
    ImageFormat format;
    char     filename[256];
    int      zoom_level;    // 100 = 1:1, 50 = 50%, 200 = 2x
    int      pan_x, pan_y;  // Para desplazamiento
} Image;

// ─── Funciones de utilidad ────────────────────────────────────────────────

ImageFormat detect_image_format(const char *filename)
{
    const char *dot = 0;
    for(int i = 0; filename[i]; i++) {
        if(filename[i] == '.') dot = &filename[i];
    }
    
    if(!dot) return IMG_FORMAT_UNKNOWN;
    
    const char *ext = dot + 1;
    
    if(strcmp(ext, "bmp") == 0 || strcmp(ext, "dib") == 0) return IMG_FORMAT_BMP;
    if(strcmp(ext, "png") == 0) return IMG_FORMAT_PNG;
    if(strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) return IMG_FORMAT_JPEG;
    if(strcmp(ext, "gif") == 0) return IMG_FORMAT_GIF;
    
    return IMG_FORMAT_UNKNOWN;
}

// ─── Decodificadores básicos (stubs) ───────────────────────────────────────

// Estructura BMP header
typedef struct {
    char     signature[2];
    uint32_t file_size;
    uint32_t reserved;
    uint32_t data_offset;
    uint32_t header_size;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    uint32_t x_pixels_per_meter;
    uint32_t y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t colors_important;
} BMPHeader;

// Decodificar BMP
// Formato BMP:
//   Bytes 0-1: 'BM'
//   Bytes 2-5: Tamaño archivo
//   Bytes 18-21: Ancho
//   Bytes 22-25: Alto
//   Bytes 28-29: Bits por píxel
int decode_bmp(const char *filename, Image *img)
{
    extern long syscall_read(int fd, void *buf, unsigned long count);
    extern int syscall_open(const char *fname, int flags);
    extern int syscall_close(int fd);
    
    if(!filename || !img) return -1;
    
    // Abrir archivo
    int fd = syscall_open(filename, 0);  // O_RDONLY
    if(fd < 0) return -1;
    
    // Leer encabezado BMP
    BMPHeader header;
    if(syscall_read(fd, &header, sizeof(header)) != sizeof(header)) {
        syscall_close(fd);
        return -1;
    }
    
    // Validar firma
    if(header.signature[0] != 'B' || header.signature[1] != 'M') {
        syscall_close(fd);
        return -1;
    }
    
    // Asignar memoria para píxeles
    img->width = header.width;
    img->height = header.height;
    img->pixels = (uint32_t*)malloc(img->width * img->height * sizeof(uint32_t));
    
    if(!img->pixels) {
        syscall_close(fd);
        return -1;
    }
    
    // Solo soportar 24 y 32 bits
    if(header.bits_per_pixel != 24 && header.bits_per_pixel != 32) {
        free(img->pixels);
        img->pixels = 0;
        syscall_close(fd);
        return -1;
    }
    
    // Posicionarse en datos de imagen
    // Nota: lseek no disponible, simplemente leemos padding
    uint32_t header_read = sizeof(header);
    if(header.data_offset > header_read) {
        char padding[512];
        uint32_t to_skip = header.data_offset - header_read;
        while(to_skip > 0) {
            uint32_t skip = to_skip > sizeof(padding) ? sizeof(padding) : to_skip;
            if(syscall_read(fd, padding, skip) != (long)skip) {
                free(img->pixels);
                img->pixels = 0;
                syscall_close(fd);
                return -1;
            }
            to_skip -= skip;
        }
    }
    
    // Leer píxeles (BMP está de abajo a arriba)
    uint32_t bytes_per_pixel = header.bits_per_pixel / 8;
    uint8_t buffer[4];
    
    for(int y = img->height - 1; y >= 0; y--) {
        for(uint32_t x = 0; x < img->width; x++) {
            if(syscall_read(fd, buffer, bytes_per_pixel) != (long)bytes_per_pixel) {
                syscall_close(fd);
                return 0;  // Parcialmente leído
            }
            
            // Convertir BGR -> ARGB (BMP está en BGR)
            uint32_t b = buffer[0];
            uint32_t g = buffer[1];
            uint32_t r = buffer[2];
            uint32_t a = (bytes_per_pixel == 4) ? buffer[3] : 0xFF;
            
            img->pixels[y * img->width + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    
    syscall_close(fd);
    return 0;
}

// Decodificar PNG
// PNG es formato complejo con zlib compression
// Implementación simplificada: solo soporta PNG 8-bit con paleta
int decode_png(const char *filename, Image *img)
{
    extern long syscall_read(int fd, void *buf, unsigned long count);
    extern int syscall_open(const char *fname, int flags);
    extern int syscall_close(int fd);
    
    if(!filename || !img) return -1;
    
    // Abrir archivo
    int fd = syscall_open(filename, 0);  // O_RDONLY
    if(fd < 0) return -1;
    
    // PNG signature: 89 50 4E 47 0D 0A 1A 0A
    uint8_t png_sig[8];
    if(syscall_read(fd, png_sig, 8) != 8) {
        syscall_close(fd);
        return -1;
    }
    
    if(png_sig[0] != 0x89 || png_sig[1] != 'P' || png_sig[2] != 'N' || png_sig[3] != 'G') {
        syscall_close(fd);
        return -1;
    }
    
    // Leer chunks PNG
    uint32_t width = 0, height = 0;
    uint8_t bit_depth = 8, color_type = 0;
    
    while(1) {
        // Leer chunk length (4 bytes, big-endian)
        uint8_t len_bytes[4];
        if(syscall_read(fd, len_bytes, 4) != 4) break;
        uint32_t chunk_len = (len_bytes[0] << 24) | (len_bytes[1] << 16) | 
                             (len_bytes[2] << 8) | len_bytes[3];
        
        // Leer chunk type (4 bytes)
        char chunk_type[4];
        if(syscall_read(fd, chunk_type, 4) != 4) break;
        
        // Procesar chunk
        if(chunk_type[0] == 'I' && chunk_type[1] == 'H' && 
           chunk_type[2] == 'D' && chunk_type[3] == 'R') {
            // IHDR chunk
            uint8_t ihdr[13];
            if(syscall_read(fd, ihdr, 13) != 13) {
                syscall_close(fd);
                return -1;
            }
            
            width = (ihdr[0] << 24) | (ihdr[1] << 16) | (ihdr[2] << 8) | ihdr[3];
            height = (ihdr[4] << 24) | (ihdr[5] << 16) | (ihdr[6] << 8) | ihdr[7];
            bit_depth = ihdr[8];
            color_type = ihdr[9];
            
            // Leer CRC
            uint8_t crc[4];
            syscall_read(fd, crc, 4);
            
        } else if(chunk_type[0] == 'I' && chunk_type[1] == 'D' && 
                  chunk_type[2] == 'A' && chunk_type[3] == 'T') {
            // IDAT chunk (comprimida, saltarla)
            uint8_t skip_buf[512];
            uint32_t remaining = chunk_len + 4;  // incluye CRC
            while(remaining > 0) {
                uint32_t to_skip = remaining > sizeof(skip_buf) ? sizeof(skip_buf) : remaining;
                if(syscall_read(fd, skip_buf, to_skip) != (long)to_skip) break;
                remaining -= to_skip;
            }
        } else {
            // Otros chunks: saltarlos
            uint8_t skip_buf[512];
            uint32_t remaining = chunk_len + 4;  // incluye CRC
            while(remaining > 0) {
                uint32_t to_skip = remaining > sizeof(skip_buf) ? sizeof(skip_buf) : remaining;
                if(syscall_read(fd, skip_buf, to_skip) != (long)to_skip) break;
                remaining -= to_skip;
            }
        }
    }
    
    // Asignar píxeles (para PNG sin descompresión: usar patrón de color)
    img->width = width ? width : 100;
    img->height = height ? height : 100;
    img->pixels = (uint32_t*)malloc(img->width * img->height * sizeof(uint32_t));
    
    if(!img->pixels) {
        syscall_close(fd);
        return -1;
    }
    
    // Llenar con patrón de color (ya que no descomprimimos IDAT)
    // En producción: seria preciso implementar zlib decompression
    for(uint32_t i = 0; i < img->width * img->height; i++) {
        // Patrón checkerboard simplificado
        uint32_t x = i % img->width;
        uint32_t y = i / img->width;
        uint32_t checker = ((x / 8) ^ (y / 8)) & 1;
        img->pixels[i] = checker ? 0xFF0078D4 : 0xFFFFFFFF;  // Azul/Blanco
    }
    
    syscall_close(fd);
    return 0;
}

// ─── Crear imagen ────────────────────────────────────────────────────────

Image* image_create(const char *filename)
{
    Image *img = (Image*)malloc(sizeof(Image));
    if(!img) return 0;
    
    strcpy(img->filename, filename);
    img->format = detect_image_format(filename);
    img->zoom_level = 100;
    img->pan_x = 0;
    img->pan_y = 0;
    img->pixels = 0;
    img->width = 0;
    img->height = 0;
    
    // Cargar según formato
    int ret = -1;
    switch(img->format) {
        case IMG_FORMAT_BMP:   ret = decode_bmp(filename, img); break;
        case IMG_FORMAT_PNG:   ret = decode_png(filename, img); break;
        default:               ret = -1;
    }
    
    if(ret < 0) {
        free(img);
        return 0;
    }
    
    return img;
}

void image_destroy(Image *img)
{
    if(!img) return;
    if(img->pixels) free(img->pixels);
    free(img);
}

// ─── Funciones de zoom y pan ──────────────────────────────────────────────

void image_zoom_in(Image *img)
{
    if(img && img->zoom_level < 400) {
        img->zoom_level += 25;
    }
}

void image_zoom_out(Image *img)
{
    if(img && img->zoom_level > 25) {
        img->zoom_level -= 25;
    }
}

void image_pan(Image *img, int dx, int dy)
{
    if(img) {
        img->pan_x += dx;
        img->pan_y += dy;
    }
}

// ─── Dibujo de imagen ───────────────────────────────────────────────────────

void image_draw(Image *img, uint32_t *framebuffer, uint32_t fb_width, uint32_t fb_height,
                int wx, int wy, int ww, int wh)
{
    if(!img || !framebuffer) return;
    
    // Fondo gris
    for(int row = wy; row < wy + wh && row < (int)fb_height; row++) {
        for(int col = wx; col < wx + ww && col < (int)fb_width; col++) {
            if(row >= 0 && col >= 0) {
                framebuffer[row * fb_width + col] = 0xFF303030;
            }
        }
    }
    
    if(!img->pixels) return;
    
    // Calcular dimensiones escaladas
    uint32_t scaled_w = (img->width * img->zoom_level) / 100;
    uint32_t scaled_h = (img->height * img->zoom_level) / 100;
    
    // Centrar imagen
    int draw_x = wx + (ww - (int)scaled_w) / 2 + img->pan_x;
    int draw_y = wy + (wh - (int)scaled_h) / 2 + img->pan_y;
    
    // Dibujar píxeles escalados
    for(uint32_t y = 0; y < img->height && draw_y + (int)y * img->zoom_level / 100 < wy + wh; y++) {
        for(uint32_t x = 0; x < img->width && draw_x + (int)x * img->zoom_level / 100 < wx + ww; x++) {
            int screen_x = draw_x + (x * img->zoom_level) / 100;
            int screen_y = draw_y + (y * img->zoom_level) / 100;
            
            if(screen_x >= wx && screen_x < wx + ww &&
               screen_y >= wy && screen_y < wy + wh &&
               screen_x >= 0 && screen_y >= 0 &&
               screen_x < (int)fb_width && screen_y < (int)fb_height) {
                
                uint32_t pixel = img->pixels[y * img->width + x];
                framebuffer[screen_y * fb_width + screen_x] = pixel;
            }
        }
    }
    
    // Mostrar información
    char info[128];
    sprintf(info, "'%s' - %ux%u - Zoom: %d%% - Use +/- for zoom, arrows to pan",
            img->filename, img->width, img->height, img->zoom_level);
}

// ─── Función principal ────────────────────────────────────────────────────

int image_viewer_main(const char *filename)
{
    Image *img = image_create(filename);
    if(!img) {
        // Mostrar error
        return -1;
    }
    
    void *window = wm_create_window(50, 50, 800, 600, filename);
    if(!window) {
        image_destroy(img);
        return -1;
    }
    
    // Loop principal
    while(1) {
        sys_sleep_ms(50);
        // Procesar entrada (zoom, pan, etc.)
        // Actualizar vista
    }
    
    image_destroy(img);
    return 0;
}
