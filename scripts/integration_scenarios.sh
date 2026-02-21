#!/bin/bash

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
    for i in {1..6}; do xdotool key --delay 500 $i; done
    xdotool key --delay 500 2 # Style: Subtle

    # 3. Post-Process Effects
    echo "=> Toggling Effects"
    xdotool key --delay 500 v # Vignette OFF
    xdotool key --delay 500 v # Vignette ON
    xdotool key --delay 500 g # Grain
    xdotool key --delay 500 b # Bloom
    xdotool key --delay 500 h # DoF
    xdotool key --delay 500 j # Auto Exposure
    xdotool key --delay 500 m # Motion Blur
    xdotool key --delay 500 x # FXAA

    xdotool key --delay 500 w # Wireframe ON
    xdotool key --delay 500 w # Wireframe OFF

    echo "=> Toggling Sphere Sorting (O)"
    for i in {1..6}; do xdotool key --delay 500 o; done

    # 4. Camera Movement
    echo "=> Moving Camera"
    for key in z d s a q e; do
        xdotool keydown $key; sleep 0.5; xdotool keyup $key
    done

    # 5. PBR Debug Modes
    echo "=> Cycling PBR Debug Modes (F5)"
    for i in {1..9}; do xdotool key --delay 300 F5; done

    # 6. Performance Mode
    echo "=> Toggling Performance Mode (F9)"
    xdotool key --delay 500 F9
    sleep 1
    xdotool key --delay 500 F9

    # 7. Style 7 - Banding Modes
    echo "=> Testing Style 7 - Banding Modes"
    for i in {1..4}; do xdotool key --delay 500 7; done

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
    for i in {1..6}; do xdotool key --delay 500 $i; done
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
    for key in z d s a; do
        xdotool keydown $key; sleep 0.5; xdotool keyup $key
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

    echo "=> Test Complete."
    xdotool key Escape
}
