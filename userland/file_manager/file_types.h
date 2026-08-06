// userland/file_manager/file_types.h
// Sistema de tipos de archivo y manejo de extensiones

#ifndef FILE_TYPES_H
#define FILE_TYPES_H

#include <stdint.h>
#include "../libc/string.h"

// ─── Comparación de strings case-insensitive ───────────────────────────────
static inline int strcmp_ci(const char *a, const char *b)
{
    if(!a || !b) return (a != b);
    while(*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if(ca != cb) return ca - cb;
        a++; b++;
    }
    return *a - *b;
}

// ─── Enumeración de tipos de archivo ────────────────────────────────────
typedef enum {
    FT_UNKNOWN,      // Desconocido
    FT_FOLDER,       // Directorio
    FT_TXT,          // Texto plano
    FT_C,            // Código C
    FT_CPP,          // Código C++
    FT_H,            // Header C/C++
    FT_PY,           // Python
    FT_JS,           // JavaScript
    FT_HTML,         // HTML
    FT_CSS,          // Estilos HTML
    FT_JSON,         // JSON
    FT_BIN,          // Binario
    FT_EXE,          // Ejecutable
    FT_IMG_JPG,      // Imagen JPEG
    FT_IMG_PNG,      // Imagen PNG
    FT_IMG_BMP,      // Imagen BMP
    FT_IMG_GIF,      // Imagen GIF
    FT_IMG_ICO,      // Icono
    FT_AUDIO_MP3,    // Audio MP3
    FT_AUDIO_WAV,    // Audio WAV
    FT_AUDIO_OGG,    // Audio OGG
    FT_VIDEO_MP4,    // Video MP4
    FT_VIDEO_AVI,    // Video AVI
    FT_VIDEO_MKV,    // Video MKV
    FT_PDF,          // PDF
    FT_ZIP,          // Archivo comprimido
    FT_FOLDER_SYS,   // Carpeta del sistema
} FileType;

// ─── Estructura para información de archivo ─────────────────────────────
typedef struct {
    char     name[256];      // Nombre del archivo
    char     path[512];      // Ruta completa
    uint32_t size;           // Tamaño en bytes
    uint32_t created;        // Timestamp creación
    uint32_t modified;       // Timestamp modificación
    FileType type;           // Tipo de archivo
    uint8_t  is_dir;         // Es directorio?
    uint8_t  is_hidden;      // Es oculto?
} FileInfo;

// ─── Funciones de utilidad ──────────────────────────────────────────────

// Determinar tipo de archivo basado en extensión
static inline FileType file_get_type(const char *filename)
{
    if(!filename) return FT_UNKNOWN;
    
    // Buscar última posición de punto
    const char *dot = 0;
    for(int i = 0; filename[i]; i++) {
        if(filename[i] == '.') dot = &filename[i];
    }
    
    if(!dot) return FT_UNKNOWN;
    
    const char *ext = dot + 1;
    
    // Comparar extensiones (case-insensitive)
    #define EXT_CMP(e, t) (strcmp_ci(ext, e) == 0 ? (t) : FT_UNKNOWN)
    
    if(strcmp_ci(ext, "txt") == 0) return FT_TXT;
    if(strcmp_ci(ext, "c") == 0) return FT_C;
    if(strcmp_ci(ext, "cpp") == 0) return FT_CPP;
    if(strcmp_ci(ext, "h") == 0) return FT_H;
    if(strcmp_ci(ext, "py") == 0) return FT_PY;
    if(strcmp_ci(ext, "js") == 0) return FT_JS;
    if(strcmp_ci(ext, "html") == 0) return FT_HTML;
    if(strcmp_ci(ext, "css") == 0) return FT_CSS;
    if(strcmp_ci(ext, "json") == 0) return FT_JSON;
    if(strcmp_ci(ext, "jpg") == 0 || strcmp_ci(ext, "jpeg") == 0) return FT_IMG_JPG;
    if(strcmp_ci(ext, "png") == 0) return FT_IMG_PNG;
    if(strcmp_ci(ext, "bmp") == 0) return FT_IMG_BMP;
    if(strcmp_ci(ext, "gif") == 0) return FT_IMG_GIF;
    if(strcmp_ci(ext, "ico") == 0) return FT_IMG_ICO;
    if(strcmp_ci(ext, "mp3") == 0) return FT_AUDIO_MP3;
    if(strcmp_ci(ext, "wav") == 0) return FT_AUDIO_WAV;
    if(strcmp_ci(ext, "ogg") == 0) return FT_AUDIO_OGG;
    if(strcmp_ci(ext, "mp4") == 0) return FT_VIDEO_MP4;
    if(strcmp_ci(ext, "avi") == 0) return FT_VIDEO_AVI;
    if(strcmp_ci(ext, "mkv") == 0) return FT_VIDEO_MKV;
    if(strcmp_ci(ext, "pdf") == 0) return FT_PDF;
    if(strcmp_ci(ext, "zip") == 0) return FT_ZIP;
    if(strcmp_ci(ext, "exe") == 0 || strcmp_ci(ext, "bin") == 0) return FT_EXE;
    
    return FT_UNKNOWN;
}

// Obtener ícono basado en tipo
static inline const char* file_get_icon(FileType type)
{
    switch(type) {
        case FT_FOLDER:     return "[D]";
        case FT_FOLDER_SYS: return "[S]";
        case FT_TXT:        return "[T]";
        case FT_C:
        case FT_CPP:
        case FT_H:
        case FT_PY:
        case FT_JS:         return "[<]";  // Code
        case FT_HTML:
        case FT_CSS:
        case FT_JSON:       return "[W]";  // Web
        case FT_IMG_JPG:
        case FT_IMG_PNG:
        case FT_IMG_BMP:
        case FT_IMG_GIF:
        case FT_IMG_ICO:    return "[I]";  // Image
        case FT_AUDIO_MP3:
        case FT_AUDIO_WAV:
        case FT_AUDIO_OGG:  return "[♪]";  // Audio
        case FT_VIDEO_MP4:
        case FT_VIDEO_AVI:
        case FT_VIDEO_MKV:  return "[▶]";  // Video
        case FT_PDF:        return "[P]";
        case FT_ZIP:        return "[Z]";
        case FT_EXE:        return "[*]";  // Executable
        default:            return "[ ]";
    }
}

// Obtener descripción de tipo
static inline const char* file_get_type_desc(FileType type)
{
    switch(type) {
        case FT_UNKNOWN:    return "Unknown";
        case FT_FOLDER:     return "Folder";
        case FT_FOLDER_SYS: return "System Folder";
        case FT_TXT:        return "Text File";
        case FT_C:          return "C Source Code";
        case FT_CPP:        return "C++ Source Code";
        case FT_H:          return "Header File";
        case FT_PY:         return "Python Script";
        case FT_JS:         return "JavaScript";
        case FT_HTML:       return "HTML Document";
        case FT_CSS:        return "CSS Stylesheet";
        case FT_JSON:       return "JSON Data";
        case FT_IMG_JPG:    return "JPEG Image";
        case FT_IMG_PNG:    return "PNG Image";
        case FT_IMG_BMP:    return "BMP Image";
        case FT_IMG_GIF:    return "GIF Image";
        case FT_IMG_ICO:    return "Icon File";
        case FT_AUDIO_MP3:  return "MP3 Audio";
        case FT_AUDIO_WAV:  return "WAV Audio";
        case FT_AUDIO_OGG:  return "OGG Audio";
        case FT_VIDEO_MP4:  return "MP4 Video";
        case FT_VIDEO_AVI:  return "AVI Video";
        case FT_VIDEO_MKV:  return "MKV Video";
        case FT_PDF:        return "PDF Document";
        case FT_ZIP:        return "ZIP Archive";
        case FT_EXE:        return "Executable";
        case FT_BIN:        return "Binary File";
        default:            return "Unknown";
    }
}

#endif // FILE_TYPES_H
