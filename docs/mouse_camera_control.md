
# Mouse Camera Control

## Overview
This document explains the implementation of the mouse-controlled camera system (First Person View) in the engine. The system allows orientation control (Pitch/Yaw) via the mouse and movement via the keyboard.

## Features
- **Capture Mode**: The cursor is hidden and locked to the window center.
- **Sensitivity**: Adjustable sensitivity factor.
- **Clamping**: Pitch is limited to +/- 89 degrees to avoid Gimbal Lock.
- **Smoothness**: Frame-rate independent movement using `deltaTime`.

## Architecture

### Data Flow
The input event management is decoupled from the camera update logic.

\dot
digraph CameraInput {
  rankdir=LR;
  bgcolor="transparent";
  dpi=72;

  // Suckless-Modern "Ghost" Design Tokens (Upscaled)
  node [
    shape=rect,
    style="rounded",
    fontname="Helvetica,Arial,sans-serif",
    fontsize=16,
    fillcolor="none",
    color="#414868",
    fontcolor="#c0caf5",
    penwidth=2
  ];

  edge [
    color="#565f89",
    fontname="Helvetica,Arial,sans-serif",
    fontsize=18,
    fontcolor="#9aa5ce",
    arrowsize=0.8,
    penwidth=1.2
  ];

  Input [label="GLFW Input\n(Mouse Move)", color="#7dcfff", fontcolor="#7dcfff"];
  Callback [label="mouse_callback()\n(Calculate Delta)", color="#9ece6a", fontcolor="#9ece6a", penwidth=3];
  Global [label="App State\n(Store Yaw/Pitch)", color="#414868"];
  Update [label="app_update()\n(Compute Front)", color="#7aa2f7", fontcolor="#7aa2f7"];
  Render [label="View Matrix\n(LookAt)", color="#bb9af7", fontcolor="#bb9af7", penwidth=3];

  Input -> Callback;
  Callback -> Global;
  Global -> Update;
  Update -> Render [color="#bb9af7", penwidth=2];
}
\enddot

## Implementation Details

### Rotation Calculation (Euler Angles)
The camera's direction vector is calculated from the Yaw and Pitch angles:

\code{.c}
// Convert Spherical to Cartesian coordinates
front.x = cos(glm_rad(yaw)) * cos(glm_rad(pitch));
front.y = sin(glm_rad(pitch));
front.z = sin(glm_rad(yaw)) * cos(glm_rad(pitch));
glm_normalize(front);
\endcode

### Input Handling
The `mouse_callback` function handles the raw mouse input:

1.  Calculate offset: `xoffset = xpos - lastX`
2.  Apply sensitivity: `xoffset *= sensitivity`
3.  Update angles: `yaw += xoffset`
4.  Constrain Pitch: `pitch = clamp(pitch, -89.0f, 89.0f)`

> [!NOTE]
> Ensure `firstMouse` check is implemented to prevent a sudden camera jump on the initial frame where the mouse enters the window.

## Usage
To enable mouse capture:
\code{.c}
// Enable mouse capture in GLFW
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
\endcode
To release the mouse (e.g., for UI interaction), switch to `GLFW_CURSOR_NORMAL`.
