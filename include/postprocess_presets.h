/**
 * @file postprocess_presets.h
 * @brief Extern declarations for predefined post-processing pipeline presets.
 *
 * Definitions live in src/postprocess_presets.c (single .rodata copy).
 */

#ifndef POSTPROCESS_PRESETS_H
#define POSTPROCESS_PRESETS_H

#include "postprocess_internal.h"

/** @brief Default balanced settings. */
extern const PostProcessPreset PRESET_DEFAULT;
/** @brief Subtle adjustments. */
extern const PostProcessPreset PRESET_SUBTLE;
/** @brief Rich, high-contrast look. */
extern const PostProcessPreset PRESET_CINEMATIC;
/** @brief Warm, grainy look. */
extern const PostProcessPreset PRESET_VINTAGE;
/** @brief Cool, green-tinted look. */
extern const PostProcessPreset PRESET_MATRIX;
/** @brief High-contrast B&W. */
extern const PostProcessPreset PRESET_BW_CONTRAST;
/** @brief Art style: Posterized. */
extern const PostProcessPreset PRESET_POSTERIZED;
/** @brief Art style: Retro Computing (Dithered). */
extern const PostProcessPreset PRESET_RETRO;
/** @brief Art style: Perceptual (Analog-like). */
extern const PostProcessPreset PRESET_ANALOG;
/** @brief Art style: Channel (VGA/CGA-like). */
extern const PostProcessPreset PRESET_CHANNEL_GFX;
/** @brief Art style: Blueprint / Hologram. */
extern const PostProcessPreset PRESET_BLUEPRINT;
/** @brief Nordic Noir: Foggy neon-lit night with teal-orange split toning. */
extern const PostProcessPreset PRESET_NORDIC_NOIR;
/** @brief Sony Alpha 7S III: Professional cinematic look. */
extern const PostProcessPreset PRESET_SONY_A7SIII;

#endif /* POSTPROCESS_PRESETS_H */
