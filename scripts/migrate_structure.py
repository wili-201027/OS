#!/usr/bin/env python3
"""
Script de migración de estructura de carpetas
Convierte la estructura plana actual a estructura modular
"""

import os
import shutil
import sys
import json
from pathlib import Path

def create_structure():
    """Crear nueva estructura de directorios"""
    
    base_path = Path("userland")
    
    # Crear directorios principales
    dirs = [
        "base/file_manager/src",
        "base/file_manager/resources",
        "base/file_manager/docs",
        "base/shell/src",
        "base/system_info/src",
        "base/settings/src",
        
        "programs/text_editor/src",
        "programs/text_editor/resources",
        "programs/text_editor/docs",
        "programs/text_editor/build",
        
        "programs/image_viewer/src",
        "programs/image_viewer/src/decoders",
        "programs/image_viewer/resources",
        "programs/image_viewer/docs",
        
        "programs/media_player/src",
        "programs/media_player/src/audio",
        "programs/media_player/src/video",
        "programs/media_player/resources",
        
        "programs/web_browser/src",
        "programs/web_browser/resources",
        
        "programs/calculator/src",
        "programs/calculator/resources",
        
        "programs/paint/src",
        "programs/paint/resources",
        
        "programs/notes/src",
        "programs/notes/resources",
        
        "ui_lib/themes",
        "ui_lib/fonts",
        
        "lib/stdlib",
        "lib/graphics",
        "lib/io",
        "lib/sys",
        
        "app_launcher",
        "config",
    ]
    
    print("[*] Creando estructura de directorios...")
    for dir_path in dirs:
        full_path = base_path / dir_path
        full_path.mkdir(parents=True, exist_ok=True)
        print(f"  ✓ {dir_path}")
    
    print("\n[+] Estructura de directorios creada exitosamente")

def migrate_files():
    """Migrar archivos a nuevas ubicaciones"""
    
    print("\n[*] Migrando archivos...")
    
    migrations = {
        # File Manager
        "userland/file_manager": [
            ("file_types.h", "userland/base/file_manager/src/file_types.h"),
            ("file_manager.h", "userland/base/file_manager/src/file_manager.h"),
            ("file_manager.c", "userland/base/file_manager/src/file_manager.c"),
            ("file_manager_integration.h", "userland/base/file_manager/file_manager_integration.h"),
        ],
        
        # Programs
        "userland/programs": [
            ("programs.h", "userland/programs/programs.h"),
            ("text_editor.c", "userland/programs/text_editor/src/text_editor.c"),
            ("image_viewer.c", "userland/programs/image_viewer/src/image_viewer.c"),
            ("media_player.c", "userland/programs/media_player/src/media_player.c"),
            ("web_browser.c", "userland/programs/web_browser/src/web_browser.c"),
        ],
        
        # UI Library
        "userland/ui_lib": [
            ("ui_components.h", "userland/ui_lib/ui_components.h"),
            ("ui_components.c", "userland/ui_lib/ui_components.c"),
        ],
        
        # App Launcher
        "userland/app_launcher": [
            ("app_launcher.h", "userland/app_launcher/app_launcher.h"),
            ("app_launcher.c", "userland/app_launcher/app_launcher.c"),
            ("package_manager.h", "userland/app_launcher/package_manager.h"),
            ("package_manager.c", "userland/app_launcher/package_manager.c"),
        ],
    }
    
    migrated = 0
    for src_dir, file_list in migrations.items():
        for src_file, dst_file in file_list:
            src_path = Path(src_dir) / src_file
            dst_path = Path(dst_file)
            
            if src_path.exists():
                os.makedirs(dst_path.parent, exist_ok=True)
                shutil.copy2(src_path, dst_path)
                print(f"  ✓ {src_file} → {dst_file}")
                migrated += 1
            else:
                print(f"  ⚠ {src_file} no encontrado")
    
    print(f"\n[+] {migrated} archivos migrados")

def create_manifests():
    """Crear archivos manifest.json para ejemplos"""
    
    print("\n[*] Creando manifests de ejemplo...")
    
    manifests = {
        "userland/programs/calculator/manifest.json": {
            "package_id": "com.gpt-os.calculator",
            "name": "Calculator",
            "version": "1.0.0",
            "description": "Basic calculator with scientific functions",
            "type": "application",
        },
        "userland/programs/paint/manifest.json": {
            "package_id": "com.gpt-os.paint",
            "name": "Paint",
            "version": "1.0.0",
            "description": "Simple drawing and painting application",
            "type": "application",
        },
        "userland/programs/notes/manifest.json": {
            "package_id": "com.gpt-os.notes",
            "name": "Notes",
            "version": "1.0.0",
            "description": "Notes and memo application",
            "type": "application",
        },
    }
    
    for manifest_path, manifest_data in manifests.items():
        os.makedirs(os.path.dirname(manifest_path), exist_ok=True)
        with open(manifest_path, 'w') as f:
            json.dump(manifest_data, f, indent=2)
        print(f"  ✓ {manifest_path}")
    
    print(f"\n[+] {len(manifests)} manifests creados")

def create_readme_files():
    """Crear archivos README en carpetas principales"""
    
    print("\n[*] Creando archivos README...")
    
    readmes = {
        "userland/base/file_manager/README.md": """# File Manager

Base file manager application for GPT-OS.
Part of the system base, not removable.

## Features
- Directory navigation
- File listing with icons
- File type detection
- Application launcher integration
""",
        "userland/programs/text_editor/README.md": """# Text Editor

Advanced text editor with syntax highlighting.

## Supported Formats
- Text files (.txt)
- Source code (.c, .cpp, .py, .js, etc.)
- Configuration files (.json, .xml, .ini)

## Features
- Syntax highlighting
- Code completion
- Find & Replace
- Undo/Redo
""",
        "userland/programs/image_viewer/README.md": """# Image Viewer

View images with zoom and transformation.

## Supported Formats
- BMP, PNG, JPG, GIF, ICO

## Features
- Zoom in/out
- Rotate image
- Apply filters
- Slideshow
""",
        "userland/programs/media_player/README.md": """# Media Player

Play audio and video files.

## Audio Formats
- MP3, WAV, OGG, FLAC

## Video Formats
- MP4, AVI, MKV, WebM

## Features
- Playback controls
- Volume control
- Playlist support
- Seek bar
""",
    }
    
    for readme_path, content in readmes.items():
        os.makedirs(os.path.dirname(readme_path), exist_ok=True)
        with open(readme_path, 'w') as f:
            f.write(content)
        print(f"  ✓ {readme_path}")
    
    print(f"\n[+] {len(readmes)} archivos README creados")

def print_migration_guide():
    """Imprimir guía de migración"""
    
    print("""
════════════════════════════════════════════════════════════════════════════════
GUÍA DE MIGRACIÓN - ESTRUCTURA MODULAR
════════════════════════════════════════════════════════════════════════════════

CAMBIOS PRINCIPALES:

1. SEPARACIÓN BASE vs PROGRAMAS
   ✓ base/file_manager/ - Siempre incluido
   ✓ programs/*/  - Instalables opcionales

2. CADA PROGRAMA ES AUTÓNOMO
   ✓ src/           - Código fuente
   ✓ resources/     - Iconos, datos
   ✓ docs/          - Documentación
   ✓ manifest.json  - Metadata del paquete

3. LIBRERÍAS COMPARTIDAS
   ✓ lib/stdlib/    - String, memory, etc.
   ✓ lib/graphics/  - Drawing, font, color
   ✓ lib/io/        - File I/O, streams
   ✓ lib/sys/       - Syscalls, memory

4. CONFIGURACIÓN CENTRALIZADA
   ✓ config/app_registry.json   - Registro de apps
   ✓ config/system_config.json  - Config del sistema

PRÓXIMOS PASOS:

1. Compilación modular:
   make programs  # Compilar solo programs base

2. Compilación de aplicaciones:
   cd userland/programs/calculator
   make

3. Instalación de paquetes:
   ./install_package path/to/package.gpt-app

4. Package management:
   gpt-os package list
   gpt-os package install <id>
   gpt-os package uninstall <id>

════════════════════════════════════════════════════════════════════════════════
""")

def main():
    print("""
╔════════════════════════════════════════════════════════════════════════════╗
║         GPT-OS FILE MANAGER - MIGRATION SCRIPT                            ║
║         Convierte estructura plana a estructura modular                    ║
╚════════════════════════════════════════════════════════════════════════════╝
""")
    
    # Crear estructura
    create_structure()
    
    # Migrar archivos
    migrate_files()
    
    # Crear manifests
    create_manifests()
    
    # Crear READMEs
    create_readme_files()
    
    # Imprimir guía
    print_migration_guide()
    
    print("""
╔════════════════════════════════════════════════════════════════════════════╗
║  ✓ MIGRACIÓN COMPLETADA                                                    ║
│                                                                             │
│  Próximos pasos:                                                            │
│  1. Revisar nueva estructura: ls -R userland/                             │
│  2. Actualizar Makefile para nueva estructura                              │
│  3. Compilar prueba: make clean && make all                               │
│  4. Testear funcionamiento                                                 │
└════════════════════════════════════════════════════════════════════════════╘
""")

if __name__ == "__main__":
    main()
