#include "action_notifier.h"

#include "log.h"
#include "ui.h"
#include "utils.h"
#include <cglm/types.h>

void action_notifier_init(ActionNotifier* notifier)
{
	if (!notifier) {
		return;
	}
	for (int i = 0; i < MAX_ACTION_NOTIFICATIONS; i++) {
		notifier->notes[i] = (ActionNotification){0};
	}
}

void action_notifier_push(ActionNotifier* notifier, const char* text,
                          float duration)
{
	if (!notifier || !text) {
		return;
	}

	/* Find an empty slot or the oldest one */
	int slot = -1;
	for (int i = 0; i < MAX_ACTION_NOTIFICATIONS; i++) {
		if (!notifier->notes[i].active) {
			slot = i;
			break;
		}
	}

	/* If all slots full, overwrite oldest (index 0) and shift others */
	if (slot == -1) {
		for (int i = 0; i < MAX_ACTION_NOTIFICATIONS - 1; i++) {
			notifier->notes[i] = notifier->notes[i + 1];
		}
		slot = MAX_ACTION_NOTIFICATIONS - 1;
	}

	ActionNotification* note = &notifier->notes[slot];
	safe_strncpy(note->text, sizeof(note->text), text,
	             MAX_ACTION_TEXT_LENGTH - 1);
	note->lifetime = 0.0F;
	note->max_lifetime = duration;
	note->active = 1;

	LOG_DEBUG("action_notifier", "Pushed: %s", text);
}

void action_notifier_update(ActionNotifier* notifier, float delta_time)
{
	if (!notifier) {
		return;
	}

	for (int i = 0; i < MAX_ACTION_NOTIFICATIONS; i++) {
		if (notifier->notes[i].active) {
			notifier->notes[i].lifetime += delta_time;
			if (notifier->notes[i].lifetime >=
			    notifier->notes[i].max_lifetime) {
				notifier->notes[i].active = 0;
			}
		}
	}
}

void action_notifier_draw(ActionNotifier* notifier, UIContext* ui_ctx,
                          int screen_width, int screen_height)
{
	if (!notifier || !ui_ctx) {
		return;
	}

	static const float START_X = 20.0F;
	static const float START_Y_FROM_BOTTOM = 60.0F;
	static const float SPACING = 25.0F;
	static const vec3 NOTIF_COLOR = {0.0F, 0.8F, 1.0F}; /* Cyan-ish */
	static const float FADE_TIME = 0.5F;

	float current_y = (float)screen_height - START_Y_FROM_BOTTOM;

	for (int i = 0; i < MAX_ACTION_NOTIFICATIONS; i++) {
		if (!notifier->notes[i].active) {
			continue;
		}

		ActionNotification* note = &notifier->notes[i];
		float remaining = note->max_lifetime - note->lifetime;
		float alpha = 1.0F;

		if (remaining < FADE_TIME) {
			alpha = remaining / FADE_TIME;
		}

		ui_draw_text_ex(ui_ctx, note->text, START_X, current_y,
		                (float*)NOTIF_COLOR, alpha, screen_width,
		                screen_height);

		current_y -= SPACING;
	}
}
