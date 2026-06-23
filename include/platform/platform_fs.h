#ifndef SUCKLESS_OGL_PLATFORM_FS_H
#define SUCKLESS_OGL_PLATFORM_FS_H

#include <stdbool.h>

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

/**
 * @brief Check if a directory exists.
 *
 * @param path Directory path.
 * @return True if the directory exists, false otherwise.
 */
bool platform_dir_exists(const char* path);

/**
 * @brief Setup the working directory relative to the executable path.
 *
 * It shifts the working directory to the directory of the executable,
 * then traverses parent directories (up to 4 levels) until it finds a directory
 * containing both 'shaders' and 'assets'.
 *
 * @param exec_path The executable path (typically argv[0]).
 */
void platform_setup_working_dir(const char* exec_path);

#endif  // SUCKLESS_OGL_PLATFORM_FS_H
