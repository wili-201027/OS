from pathlib import Path
import re

base = Path(r'e:\os')
files = [
    base / 'kernel' / 'kernel.c',
    base / 'userland' / 'compositor' / 'compositor.c',
    base / 'userland' / 'compositor' / 'glass_renderer.c',
    base / 'userland' / 'compositor' / 'input_router.c',
    base / 'userland' / 'compositor' / 'window_manager.c',
    base / 'userland' / 'servers' / 'device_server.c',
    base / 'userland' / 'servers' / 'fs_server.c',
    base / 'userland' / 'servers' / 'terminal_server.c',
]

for f in files:
    text = f.read_text(encoding='utf-8')
    orig = text
    # Remove extern "C" blocks and declarations
    text = re.sub(r'extern \"C\"\s*\{', '', text)
    text = text.replace('extern "C"', '')
    text = text.replace('extern "C"', '')
    text = text.replace('NULL', 'NULL')  # no-op, keep for readability
    text = text.replace('nullptr', 'NULL')

    if f.name == 'window_manager.c':
        # Convert reference API to pointer-based C style
        text = re.sub(r'static const VisualTheme &get_active_theme\s*\(\s*\)',
                      'static const VisualTheme *get_active_theme(void)', text)
        text = re.sub(r'const VisualTheme &theme = get_active_theme\(\);',
                      'const VisualTheme *theme = get_active_theme();', text)
        text = text.replace('theme.', 'theme->')
        text = text.replace('return *s_active_theme;', 'return s_active_theme;')

    # Remove default parameter syntax in draw_char_fb
    text = re.sub(r'static void draw_char_fb\(([^\)]*?)int scale=1\)',
                  r'static void draw_char_fb(\1int scale)', text)

    # Remove any stray C++ comments that are okay in C, but keep unchanged.

    if text != orig:
        f.write_text(text, encoding='utf-8')
        print(f'Updated {f}')
