/**
 * @file cli.h
 * @brief Command-line interface and argument parsing.
 */

#ifndef CLI_H
#define CLI_H

/**
 * @enum CliAction
 * @brief Directive for the application after parsing arguments.
 */
typedef enum {
	CLI_ACTION_CONTINUE,     /**< Arguments parsed, proceed to start
	                            application. */
	CLI_ACTION_EXIT_SUCCESS, /**< Help or version printed, exit normally. */
	CLI_ACTION_EXIT_FAILURE /**< Invalid argument detected, exit with error.
	                         */
} CliAction;

/**
 * @brief Parses command-line arguments and determines the next action.
 *
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 * @return The directive for the main entry point.
 *
 * @see main.c
 */
CliAction cli_handle_args(int argc, char* argv[]);

#endif /* CLI_H */
