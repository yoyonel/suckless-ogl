#include "cli.h"

#include <stdio.h>
#include <string.h>

static void print_help(const char* prog_name)
{
	(void)printf("Usage: %s [options]\n\n", prog_name);
	(void)printf("Options:\n");
	(void)printf("  -h, --help      Show this help message and exit\n");
	(void)printf("  --api <name>    Choose graphics API (opengl, vulkan)\n\n");
	(void)printf("Environment Variables:\n");
	(void)printf("  OGL_LOG_LEVEL   Set the logging level\n");
	(void)printf(
	    "                  (DEBUG, INFO, WARNING, ERROR, CRITICAL)\n");
	(void)printf("                  Default: INFO\n");
}

CliResult cli_handle_args(int argc, char* argv[])
{
	CliResult result = {CLI_ACTION_CONTINUE, API_OPENGL};

	if (argc <= 1) {
		return result;
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			print_help(argv[0]);
			result.action = CLI_ACTION_EXIT_SUCCESS;
			return result;
		} else if (strcmp(argv[i], "--api") == 0) {
			if (i + 1 < argc) {
				const char* api_name = argv[i + 1];
				if (strcmp(api_name, "opengl") == 0) {
					result.api = API_OPENGL;
				} else if (strcmp(api_name, "vulkan") == 0) {
					result.api = API_VULKAN;
				} else {
					(void)fprintf(stderr,
					              "Error: Unknown API '%s'\n\n",
					              api_name);
					print_help(argv[0]);
					result.action = CLI_ACTION_EXIT_FAILURE;
					return result;
				}
				i++; /* Skip next argument */
			} else {
				(void)fputs("Error: Missing argument for --api\n\n",
				            stderr);
				print_help(argv[0]);
				result.action = CLI_ACTION_EXIT_FAILURE;
				return result;
			}
		} else {
			/* Unrecognized option */
			(void)fputs("Error: Unknown option '", stderr);
			(void)fputs(argv[i], stderr);
			(void)fputs("'\n\n", stderr);
			print_help(argv[0]);
			result.action = CLI_ACTION_EXIT_FAILURE;
			return result;
		}
	}

	return result;
}
