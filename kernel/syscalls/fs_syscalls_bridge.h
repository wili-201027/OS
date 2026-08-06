// kernel/syscalls/fs_syscalls_bridge.h
// Bridge para conectar syscalls con VFS actual

#ifndef FS_SYSCALLS_BRIDGE_H
#define FS_SYSCALLS_BRIDGE_H

#include "fs_syscalls.h"
#include "../fs/vfs.h"

// Este archivo actúa como intermediario entre el nuevo sistema
// de syscalls del file manager y el VFS existente del kernel

// Notas de implementación:
//
// 1. sys_listdir debe llamar a vfs_readdir() del kernel
// 2. sys_stat debe llamar a vfs_stat() o similar
// 3. Los tipos FileInfo del file_manager deben mapearse a los del kernel
// 4. Los DirEntry / FileStat deben ser compatibles con las estructuras del VFS

// Implementación de funciones adaptadoras que convierten entre
// las estructuras del file_manager y las del VFS del kernel:
//
// Función adaptadora: fileinfo_from_dirent(struct dirent *de, FileInfo *fi)
// - Convierte struct dirent del kernel a FileInfo del file_manager
// - Copia nombre, tipo (directorio/archivo), tamaño, permisos
//
// Función adaptadora: fileinfo_from_stat(struct stat *st, FileInfo *fi)
// - Convierte struct stat del kernel a FileInfo del file_manager
// - Extrae información del inodo del VFS
//
// Estas funciones se usarían en sys_listdir y sys_stat para
// pasar información al userland en formato compatible

#endif // FS_SYSCALLS_BRIDGE_H
