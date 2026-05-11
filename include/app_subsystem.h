#ifndef APP_SUBSYSTEM_H
#define APP_SUBSYSTEM_H

/**
 * @file app_subsystem.h
 * @brief Subsystem descriptor pattern for structured init/cleanup.
 *
 * A SubsystemDescriptor is a {name, init, cleanup} triple.
 * app_subsystems_init() walks the table forward, calling each init();
 * on failure it calls cleanup() in reverse for all previously-succeeded
 * entries.  app_subsystems_cleanup() walks the table in reverse,
 * calling every non-NULL cleanup().
 */

struct App; /* forward — avoids pulling in app.h */

/**
 * @struct SubsystemDescriptor
 * @brief Describes one subsystem's lifecycle hooks.
 */
typedef struct {
	const char* name; /**< Human-readable name (for logs). */
	int (*init)(
	    struct App* app); /**< Returns 1 on success, 0 on failure. */
	void (*cleanup)(struct App* app); /**< NULL-safe, idempotent. */
} SubsystemDescriptor;

/**
 * @brief Walk @p table forward calling init() until the sentinel {0}.
 *
 * On the first failure at index N, cleanup() is called in reverse
 * for entries [N-1 .. 0], then 0 is returned.
 * The table must be terminated by a {0} sentinel entry.
 *
 * @return 1 if all init() calls succeed, 0 on first failure.
 */
int app_subsystems_init(struct App* app, const SubsystemDescriptor* table);

/**
 * @brief Walk @p table in reverse, calling cleanup() for each entry.
 *
 * Skips entries whose cleanup function pointer is NULL.
 * The table must be terminated by a {0} sentinel entry.
 * Intended for the normal shutdown path after a successful init.
 */
void app_subsystems_cleanup(struct App* app, const SubsystemDescriptor* table);

#endif /* APP_SUBSYSTEM_H */
