#ifndef CLI_H
#define CLI_H

/**
 * Handles command-line arguments.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line argument strings.
 * @return EXIT_SUCCESS if arguments were handled and execution should continue,
 *         EXIT_SUCCESS if a handled option (like --help) was found and the
 *         application should exit normally,
 *         EXIT_FAILURE if an error occurred (like unknown argument).
 *
 * Note: If the function returns success but the application should exit (e.g.,
 * after --help), it's expected that the caller handles this. We use a return
 * code indicating status.
 */

/* Action to take after parsing arguments */
typedef enum {
	CLI_ACTION_CONTINUE,
	CLI_ACTION_EXIT_SUCCESS,
	CLI_ACTION_EXIT_FAILURE
} CliAction;

CliAction cli_handle_args(int argc, char* argv[]);

#endif
