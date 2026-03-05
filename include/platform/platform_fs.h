#ifndef SUCKLESS_OGL_PLATFORM_FS_H
#define SUCKLESS_OGL_PLATFORM_FS_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Callback for directory enumeration.
 * @param filename File name (not full path).
 * @param is_dir True if the item is a directory.
 * @param user_data User-provided context.
 */
typedef void (*PlatformDirCallback)(const char* filename, bool is_dir,
                                    void* user_data);

/**
 * @brief Enumerate files in a directory.
 *
 * @param path Directory path.
 * @param callback Function called for each item found.
 * @param user_data User-provided context.
 * @return True on success, false otherwise.
 */
bool platform_dir_list(const char* path, PlatformDirCallback callback,
                       void* user_data);

#endif  // SUCKLESS_OGL_PLATFORM_FS_H
