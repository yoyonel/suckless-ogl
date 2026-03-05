#include "platform/platform_fs.h"

#include "utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#endif

bool platform_dir_list(const char* path, PlatformDirCallback callback,
                       void* user_data)
{
	if (!path || !callback) {
		return false;
	}

#ifdef _WIN32
	char search_path[MAX_PATH];
	snprintf(search_path, sizeof(search_path), "%s\\*", path);

	WIN32_FIND_DATAA find_data;
	HANDLE h_find = FindFirstFileA(search_path, &find_data);

	if (h_find == INVALID_HANDLE_VALUE) {
		return false;
	}

	do {
		if (strcmp(find_data.cFileName, ".") != 0 &&
		    strcmp(find_data.cFileName, "..") != 0) {
			bool is_dir = (find_data.dwFileAttributes &
			               FILE_ATTRIBUTE_DIRECTORY) != 0;
			callback(find_data.cFileName, is_dir, user_data);
		}
	} while (FindNextFileA(h_find, &find_data));

	FindClose(h_find);
	return true;
#else
	DIR* dir_handle = opendir(path);
	if (!dir_handle) {
		return false;
	}

	struct dirent* entry = NULL;
	while ((entry = readdir(dir_handle)) != NULL) {
		if (strcmp(entry->d_name, ".") != 0 &&
		    strcmp(entry->d_name, "..") != 0) {
			bool is_dir = false;
#ifdef _DIRENT_HAVE_D_TYPE
			if (entry->d_type != DT_UNKNOWN) {
				is_dir = (entry->d_type == DT_DIR);
			} else {
#endif
				// Fallback to stat if d_type is not available
				enum { MAX_PATH_SIZE = 1024 };
				char full_path[MAX_PATH_SIZE];
				if (safe_snprintf(full_path, sizeof(full_path),
				                  "%s/%s", path,
				                  entry->d_name) < 0) {
					continue;
				}
				struct stat stat_buf;
				if (stat(full_path, &stat_buf) == 0) {
					is_dir = S_ISDIR(stat_buf.st_mode);
				}
#ifdef _DIRENT_HAVE_D_TYPE
			}
#endif
			callback(entry->d_name, is_dir, user_data);
		}
	}

	closedir(dir_handle);
	return true;
#endif
}
