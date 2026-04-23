#!/bin/bash

# Detect keyboard layout for xdotool keysym mapping.
# GLFW maps keys by physical position (US QWERTY), not by keysym.
# xdotool resolves keysym names to keycodes, so on non-US layouts
# we must use the layout-specific keysym for the correct physical key.
_layout=$(setxkbmap -query 2>/dev/null | awk '/layout/{print $2}')
case "$_layout" in
    fr)
        # AZERTY: physical key positions differ from US QWERTY.
        # Variable names = GLFW_KEY constant the app expects.
        # Values = xdotool keysym that hits the correct physical key.
        KEY_A="q"              # AC01 (phys A) has keysym 'q'
        KEY_Q="a"              # AD01 (phys Q) has keysym 'a'
        KEY_W="z"              # AD02 (phys W) has keysym 'z'
        KEY_M="comma"          # AB07 (phys M) has keysym 'comma'
        KEY_COMMA="semicolon"  # AB08 (phys ,) has keysym 'semicolon'
        KEY_PERIOD="colon"     # AB09 (phys .) has keysym 'colon'
        ;;
    *)
        KEY_A="a"
        KEY_Q="q"
        KEY_W="w"
        KEY_M="m"
        KEY_COMMA="comma"
        KEY_PERIOD="period"
        ;;
esac

run_scenario_full() {
    echo "Starting Integration Test Scenario (FULL)..."

    # 0. Activate Functions
    echo "=> FPS Counter (F1)"
    for i in {1..6}; do xdotool key --delay 200 F1; done
    sleep 1

    echo "=> Help (F2)"
    xdotool key --delay 200 F2
    sleep 1
    xdotool key --delay 200 F2

    echo "=> Activate GPU Profiler (F3)"
    xdotool key --delay 200 F3
    sleep 1

    echo "=> Log on GPU Metrics (F4)"
    xdotool key --delay 200 F4
    sleep 2
    xdotool key --delay 200 F4
    sleep 1

    echo "=> Resetting Camera (R)"
    xdotool key --delay 200 R
    sleep 1

    echo "=> Test Full Screen (F)"
    xdotool key --delay 200 F
    sleep 1
    echo "=> Test Windowed (F)"
    xdotool key --delay 200 F
    sleep 1

    echo "=> Test Transition switching mode (T)"
    xdotool key --delay 200 T
    sleep 1

    # 1. Environment Switching
    echo "=> Switching Environments (Page_Up / Page_Down)"
    xdotool key --delay 200 Page_Up
    sleep 1
    xdotool key --delay 200 Page_Up
    sleep 1
    xdotool key --delay 200 Page_Down
    sleep 1

    # 2. Styles
    echo "=> Testing Styles (1-6)"
    for i in {1..6}; do xdotool key --delay 500 "$i"; done
    xdotool key --delay 500 2 # Style: Subtle

    # 8.b GI Diffuse 1-Bounce
    echo "=> Testing GI Diffuse 1-Bounce"
    for i in {1..3}; do xdotool key --delay 500 y; done

    # 8.a Debug view on GI
    echo "=> Debug view on GI"
    xdotool key --delay 500 shift+y
    sleep 1
    xdotool key --delay 500 shift+y

    # 3. Post-Process Effects
    echo "=> Toggling Effects"
    xdotool key --delay 500 v # Vignette OFF
    xdotool key --delay 500 v # Vignette ON
    xdotool key --delay 500 g # Grain
    xdotool key --delay 500 b # Bloom
    xdotool key --delay 500 h # DoF
    xdotool key --delay 500 j # Auto Exposure
    xdotool key --delay 500 "$KEY_M" # Motion Blur
    xdotool key --delay 500 x # FXAA

    xdotool key --delay 500 "$KEY_W" # Wireframe ON
    xdotool key --delay 500 "$KEY_W" # Wireframe OFF

    echo "=> Toggling Sphere Sorting (O)"
    for i in {1..6}; do xdotool key --delay 500 o; done

    # 4. Camera Movement
    echo "=> Moving Camera"
    for key in "$KEY_W" d s "$KEY_A" "$KEY_Q" e; do
        xdotool keydown "$key"; sleep 0.5; xdotool keyup "$key"
    done

    # 5. PBR Debug Modes
    echo "=> Cycling PBR Debug Modes (F5)"
    for i in {1..10}; do xdotool key --delay 300 F5; done

    # 6. Performance Mode
    echo "=> Toggling Performance Mode (F9)"
    xdotool key --delay 500 F9
    sleep 1
    xdotool key --delay 500 F9

    # 7. Style 7 - Banding Modes
    echo "=> Testing Style 7 - Banding Modes"
    for i in {1..4}; do xdotool key --delay 500 7; done

    # 8. Fog (F7)
    echo "=> Toggling Fog (F7)"
    xdotool key --delay 500 F7
    sleep 1
    echo "=> Fog Debug (Shift+F7)"
    xdotool key --delay 500 shift+F7
    sleep 1
    xdotool key --delay 500 shift+F7
    sleep 0.5
    xdotool key --delay 500 F7

    # 9. Sony A7S III / 3D LUT Pipeline (F8)
    echo "=> Activating Sony A7S III Profile (F8)"
    xdotool key --delay 500 F8
    sleep 1

    echo "=> Cycling 3D LUTs (Shift+F8)"
    for i in {1..7}; do xdotool key --delay 500 shift+F8; done
    sleep 1

    echo "=> Toggle LUT Viz (Shift+F10)"
    xdotool key --delay 500 shift+F10
    sleep 1
    xdotool key --delay 500 shift+F10

    echo "=> Resetting to Default (0)"
    xdotool key --delay 500 0

    # 10. N-Body Gravity Sandbox (Shift+G)
    echo "=> Activating N-Body Mode (Shift+G)"
    xdotool key --delay 500 shift+g
    sleep 1

    echo "=> Sim Speed Up (period) / Down (comma)"
    xdotool key --delay 300 "$KEY_PERIOD"
    xdotool key --delay 300 "$KEY_PERIOD"
    sleep 0.5
    xdotool key --delay 300 "$KEY_COMMA"
    sleep 0.5

    echo "=> Gravity Control (Shift+period / Shift+comma)"
    xdotool key --delay 300 "shift+$KEY_PERIOD"
    xdotool key --delay 300 "shift+$KEY_PERIOD"
    sleep 0.5
    xdotool key --delay 300 "shift+$KEY_COMMA"
    sleep 0.5

    echo "=> Time Reversal (Ctrl+Shift+G)"
    xdotool key --delay 300 ctrl+shift+g
    sleep 1.5
    echo "=> Time Forward (Ctrl+Shift+G)"
    xdotool key --delay 300 ctrl+shift+g
    sleep 1.5

    echo "=> Deactivating N-Body Mode (Shift+G)"
    xdotool key --delay 500 shift+g
    sleep 0.5

    echo "=> Test Complete."
    xdotool key Escape
}

run_scenario_minimal() {
    echo "Starting Integration Test Scenario (MINIMAL - for Valgrind)..."
    sleep 1

    # 1. Environment Switching
    echo "=> Switching Environments (Page_Up / Page_Down)"
    xdotool key --delay 200 Page_Up
    sleep 2
    xdotool key --delay 200 Page_Up
    sleep 2
    xdotool key --delay 200 Page_Down
    sleep 2

    # 2. Styles
    echo "=> Testing Styles (1-6)"
    for i in {1..6}; do xdotool key --delay 500 "$i"; done
    xdotool key --delay 500 2 # Style: Subtle

    # 3. Post-Process Effects
    echo "=> Toggling Effects"
    xdotool key --delay 500 v # Vignette OFF
    xdotool key --delay 500 v # Vignette ON
    xdotool key --delay 500 g # Grain
    xdotool key --delay 500 b # Bloom
    xdotool key --delay 500 h # DoF
    xdotool key --delay 500 j # Auto Exposure

    xdotool key --delay 500 w # Wireframe ON
    xdotool key --delay 500 w # Wireframe OFF

    echo "=> Toggling Sphere Sorting (O)"
    for i in {1..3}; do xdotool key --delay 800 o; done

    # 4. Camera Movement
    echo "=> Moving Camera"
    for key in "$KEY_W" d s "$KEY_A"; do
        xdotool keydown "$key"; sleep 0.5; xdotool keyup "$key"
    done

    # 5. PBR Debug Modes
    echo "=> Cycling PBR Debug Modes (F5)"
    for i in {1..5}; do xdotool key --delay 300 F5; done

    # 6. Performance Mode
    echo "=> Toggling Performance Mode (F9)"
    xdotool key --delay 500 F9
    sleep 2
    xdotool key --delay 500 F9
    sleep 1

    # 7. Sony A7S III / 3D LUT Pipeline (F8)
    echo "=> Activating Sony A7S III Profile (F8)"
    xdotool key --delay 500 F8
    sleep 2

    echo "=> Cycling 3D LUTs (Shift+F8)"
    for i in {1..3}; do xdotool key --delay 800 shift+F8; done
    sleep 1

    echo "=> Toggle LUT Viz (Shift+F10)"
    xdotool key --delay 500 shift+F10
    sleep 1
    xdotool key --delay 500 shift+F10

    echo "=> Resetting to Default (0)"
    xdotool key --delay 500 0
    sleep 1

    # 8. N-Body Gravity Sandbox (Shift+G)
    echo "=> Activating N-Body Mode (Shift+G)"
    xdotool key --delay 500 shift+g
    sleep 1

    echo "=> Sim Speed Up (period)"
    xdotool key --delay 300 "$KEY_PERIOD"
    sleep 0.5

    echo "=> Time Reversal (Ctrl+Shift+G)"
    xdotool key --delay 300 ctrl+shift+g
    sleep 1.5

    echo "=> Deactivating N-Body Mode (Shift+G)"
    xdotool key --delay 500 shift+g
    sleep 0.5

    echo "=> Test Complete."
    xdotool key Escape
}
