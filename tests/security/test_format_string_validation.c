#include "log.h"
#include <stdio.h>

/**
 * This test is INTENDED TO FAIL COMPILATION (or emit warnings).
 * The goal is to verify that the __attribute__((format...)) on log_message
 * correctly catches mismatched arguments.
 */
int main(void)
{
	// Mismatch: %d expects int, but "string" is passed.
	// This should trigger -Wformat (and -Werror=format-security if
	// enabled).
	log_message(LOG_LEVEL_INFO, "TEST", "This is a number: %d", "string");
	return 0;
}
