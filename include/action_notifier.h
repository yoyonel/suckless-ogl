/**
 * @file action_notifier.h
 * @brief Temporary on-screen notification system for user actions.
 */

#ifndef ACTION_NOTIFIER_H
#define ACTION_NOTIFIER_H

#include "ui.h"

/** @brief Maximum number of concurrent notifications. */
#define MAX_ACTION_NOTIFICATIONS 5

/** @brief Maximum length of a notification message. */
#define MAX_ACTION_TEXT_LENGTH 128

/** @brief Short notification duration (seconds). */
#define NOTIF_DUR_SHORT 1.0F

/** @brief Normal notification duration (seconds). */
#define NOTIF_DUR_NORMAL 1.5F

/** @brief Long notification duration (seconds). */
#define NOTIF_DUR_LONG 2.0F

/**
 * @struct ActionNotification
 * @brief Represents a single active notification.
 */
typedef struct {
	char text[MAX_ACTION_TEXT_LENGTH];
	float lifetime;     /**< Current time active (seconds). */
	float max_lifetime; /**< Total time to display (seconds). */
	int active;         /**< Whether this slot is occupied. */
} ActionNotification;

/**
 * @struct ActionNotifier
 * @brief Manager for action notifications.
 */
typedef struct {
	ActionNotification notes[MAX_ACTION_NOTIFICATIONS];
} ActionNotifier;

/**
 * @brief Initializes the notifier.
 * @param notifier Pointer to the notifier struct.
 */
void action_notifier_init(ActionNotifier* notifier);

/**
 * @brief Adds a new notification message.
 * @param notifier Pointer to the notifier.
 * @param text The message to display.
 * @param duration Duration in seconds.
 */
void action_notifier_push(ActionNotifier* notifier, const char* text,
                          float duration);

/**
 * @brief Adds a formatted notification message.
 * @param notifier Pointer to the notifier.
 * @param duration Duration in seconds.
 * @param format Printf-style format string.
 * @param ... Format arguments.
 */
void action_notifier_pushf(ActionNotifier* notifier, float duration,
                           const char* format, ...);

/**
 * @brief Updates notification timers.
 * @param notifier Pointer to the notifier.
 * @param delta_time Delta time since last update (seconds).
 */
void action_notifier_update(ActionNotifier* notifier, float delta_time);

/**
 * @brief Renders active notifications to the screen.
 * @param notifier Pointer to the notifier.
 * @param ui_ctx UIContext to use for drawing.
 * @param screen_width Current window width.
 * @param screen_height Current window height.
 */
void action_notifier_draw(ActionNotifier* notifier, UIContext* ui_ctx,
                          int screen_width, int screen_height);

#endif /* ACTION_NOTIFIER_H */
