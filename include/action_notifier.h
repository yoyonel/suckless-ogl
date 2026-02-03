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
 * @brief Updates notification timers.
 * @param notifier Pointer to the notifier.
 * @param dt Delta time since last update (seconds).
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
