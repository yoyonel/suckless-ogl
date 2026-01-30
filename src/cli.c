#include "cli.h"

#include <stdio.h>
#include <string.h>

static void print_help(const char* prog_name)
{
	(void)printf("Usage: %s [options]\n\n", prog_name);
	(void)printf("Options:\n");
	(void)printf("  -h, --help      Show this help message and exit\n\n");
	(void)printf("Environment Variables:\n");
	(void)printf("  OGL_LOG_LEVEL   Set the logging level\n");
	(void)printf(
	    "                  (DEBUG, INFO, WARNING, ERROR, CRITICAL)\n");
	(void)printf("                  Default: INFO\n");
}

CliAction cli_handle_args(int argc, char* argv[])
{
	if (argc <= 1) {
		return CLI_ACTION_CONTINUE;
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			print_help(argv[0]);
			return CLI_ACTION_EXIT_SUCCESS;
		}

		/* Unrecognized option */
		(void)fprintf(stderr, "Error: Unknown option '%s'\n\n",
		              argv[i]);
		print_help(argv[0]);
		return CLI_ACTION_EXIT_FAILURE;
	}

	return CLI_ACTION_CONTINUE;
}
