#include "platform/platform_fs.h"

#include "utils.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define chdir _chdir
#else
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
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

bool platform_dir_exists(const char* path)
{
	if (!path) {
		return false;
	}

#ifdef _WIN32
	DWORD dwAttrib = GetFileAttributesA(path);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
	        (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) != 0);
#else
	struct stat stat_buf;
	if (stat(path, &stat_buf) == 0) {
		return S_ISDIR(stat_buf.st_mode);
	}
	return false;
#endif
}

void platform_setup_working_dir(const char* exec_path)
{
	if (!exec_path) {
		return;
	}

	char path_buf[4096];
	safe_strncpy(path_buf, sizeof(path_buf), exec_path, strlen(exec_path));

	char* last_backslash = strrchr(path_buf, '\\');
	char* last_slash = strrchr(path_buf, '/');
	char* last_sep =
	    (last_backslash > last_slash) ? last_backslash : last_slash;

	if (last_sep) {
		*last_sep =
		    '\0';  // On coupe la chaîne juste avant l'exécutable
		if (chdir(path_buf) != 0) {
			// Ignoré silencieusement en cas d'erreur
		}
	}

	// Trouver le dossier contenant 'shaders' et 'assets' en remontant
	// les parents (utile pour le développement où le binaire est dans
	// build/)
	for (int i = 0; i < 4; ++i) {
		if (platform_dir_exists("shaders") &&
		    platform_dir_exists("assets")) {
			break;
		}
		if (chdir("..") != 0) {
			break;
		}
	}
}
