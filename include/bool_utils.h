#ifndef BOOL_UTILS_H
#define BOOL_UTILS_H

/**
 * @file bool_utils.h
 * @brief Portable bool↔int conversion macros for clang-tidy compliance.
 *
 * C's `!` operator, `||`/`&&`, and ternary conditions return `int`,
 * which triggers `readability-implicit-bool-conversion` when assigned
 * to or used with `bool` fields.  These macros centralize the casts
 * so the workaround lives in one place and can be revisited if C23
 * or a future clang-tidy relaxation removes the need.
 */

/**
 * @brief Toggle a bool lvalue (equivalent to `x = !x`).
 *
 * `!x` returns int in C; direct assignment to bool triggers the check.
 */
#define BOOL_TOGGLE(lval) ((lval) = ((!(lval)) != 0))

/**
 * @brief Convert a bool to int explicitly.
 *
 * Use in ternary conditions, arithmetic expressions, and GPU uniform
 * uploads where an int/GLint/int32_t is expected from a bool source.
 */
#define BOOL_TO_INT(b) ((int)(b))

/**
 * @brief Convert an int expression to bool explicitly.
 *
 * Use for external API returns (GLFW, OpenGL) and logical OR/AND
 * chains assigned to a bool variable.
 */
#define INT_TO_BOOL(expr) ((expr) != 0)

#endif /* BOOL_UTILS_H */
