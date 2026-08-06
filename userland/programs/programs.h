// userland/programs/programs.h
// Header para incluir todos los programas disponibles

#ifndef PROGRAMS_H
#define PROGRAMS_H

#include <stdint.h>

// ─── Funciones principales de programas ───────────────────────────────────

// Text Editor
int text_editor_main(const char *filename);

// Image Viewer
int image_viewer_main(const char *filename);

// Media Player
int media_player_main(const char *filename);

// Web Browser
int web_browser_main(const char *url);

#endif // PROGRAMS_H
