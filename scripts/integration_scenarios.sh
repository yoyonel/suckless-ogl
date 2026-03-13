#!/bin/bash
# scripts/integration_scenarios.sh
# Scenarios for controlling the application via xdotool

# Source shared utilities for send_key
SCRIPT_DIR=$(dirname "$0")
source "$SCRIPT_DIR/integration_utils.sh"

run_scenario_full() {
    local wid=$1
    if [ -z "$wid" ]; then
        echo "Error: Scenario requires a Window ID (WID)"
        return 1
    fi

    echo "Starting Integration Test Scenario (FULL) on window $wid..."

    # 0. Activate Functions
    echo "=> FPS Counter (F1)"
    for i in {1..6}; do send_key "$wid" F1 200; done
    sleep 1

    echo "=> Help (F2)"
    send_key "$wid" F2 200
    sleep 1
    send_key "$wid" F2 200

    echo "=> Activate GPU Profiler (F3)"
    send_key "$wid" F3 200
    sleep 1

    echo "=> Log on GPU Metrics (F4)"
    send_key "$wid" F4 200
    sleep 1

    echo "=> Resetting Camera (R)"
    send_key "$wid" R 200
    sleep 1

    echo "=> Test Full Screen (F)"
    send_key "$wid" F 200
    sleep 1
    echo "=> Test Windowed (F)"
    send_key "$wid" F 200
    sleep 1

    echo "=> Test Transition switching mode (T)"
    send_key "$wid" T 200
    sleep 1

    # 1. Environment Switching
    echo "=> Switching Environments (Page_Up / Page_Down)"
    send_key "$wid" Page_Up 200
    sleep 1
    send_key "$wid" Page_Up 200
    sleep 1
    send_key "$wid" Page_Down 200
    sleep 1

    # 2. Styles
    echo "=> Testing Styles (1-6)"
    for i in {1..6}; do send_key "$wid" $i 500; done
    send_key "$wid" 2 500 # Style: Subtle

    # 8.b GI Diffuse 1-Bounce
    echo "=> Testing GI Diffuse 1-Bounce"
    for i in {1..3}; do send_key "$wid" y 500; done

    # 8.a Debug view on GI
    echo "=> Debug view on GI"
    send_key "$wid" shift+y 500
    sleep 1
    send_key "$wid" shift+y 500

    # 3. Post-Process Effects
    echo "=> Toggling Effects"
    send_key "$wid" v 500 # Vignette OFF
    send_key "$wid" v 500 # Vignette ON
    send_key "$wid" g 500 # Grain
    send_key "$wid" b 500 # Bloom
    send_key "$wid" h 500 # DoF
    send_key "$wid" j 500 # Auto Exposure
    send_key "$wid" m 500 # Motion Blur
    send_key "$wid" x 500 # FXAA

    send_key "$wid" w 500 # Wireframe ON
    send_key "$wid" w 500 # Wireframe OFF

    echo "=> Toggling Sphere Sorting (O)"
    for i in {1..6}; do send_key "$wid" o 500; done

    # 4. Camera Movement
    echo "=> Moving Camera"
    for key in z d s a q e; do
        send_keydown "$wid" $key; sleep 0.5; send_keyup "$wid" $key
    done

    # 5. PBR Debug Modes
    echo "=> Cycling PBR Debug Modes (F5)"
    for i in {1..10}; do send_key "$wid" F5 300; done

    # 6. Performance Mode
    echo "=> Toggling Performance Mode (F9)"
    send_key "$wid" F9 500
    sleep 1
    send_key "$wid" F9 500

    # 7. Style 7 - Banding Modes
    echo "=> Testing Style 7 - Banding Modes"
    for i in {1..4}; do send_key "$wid" 7 500; done

    echo "=> Test Complete."
    sleep 1
    send_key "$wid" Escape 200
}

run_scenario_minimal() {
    local wid=$1
    if [ -z "$wid" ]; then
        echo "Error: Scenario requires a Window ID (WID)"
        return 1
    fi

    echo "Starting Integration Test Scenario (MINIMAL - for Valgrind) on window $wid..."
    sleep 1

    # 1. Environment Switching
    echo "=> Switching Environments (Page_Up / Page_Down)"
    send_key "$wid" Page_Up 200
    sleep 2
    send_key "$wid" Page_Up 200
    sleep 2
    send_key "$wid" Page_Down 200
    sleep 2

    # 2. Styles
    echo "=> Testing Styles (1-6)"
    for i in {1..6}; do send_key "$wid" $i 500; done
    send_key "$wid" 2 500 # Style: Subtle

    # 3. Post-Process Effects
    echo "=> Toggling Effects"
    send_key "$wid" v 500 # Vignette OFF
    send_key "$wid" v 500 # Vignette ON
    send_key "$wid" g 500 # Grain
    send_key "$wid" b 500 # Bloom
    send_key "$wid" h 500 # DoF
    send_key "$wid" j 500 # Auto Exposure

    send_key "$wid" w 500 # Wireframe ON
    send_key "$wid" w 500 # Wireframe OFF

    echo "=> Toggling Sphere Sorting (O)"
    for i in {1..3}; do send_key "$wid" o 800; done

    # 4. Camera Movement
    echo "=> Moving Camera"
    for key in z d s a; do
        send_keydown "$wid" $key; sleep 0.5; send_keyup "$wid" $key
    done

    # 5. PBR Debug Modes
    echo "=> Cycling PBR Debug Modes (F5)"
    for i in {1..5}; do send_key "$wid" F5 300; done

    # 6. Performance Mode
    echo "=> Toggling Performance Mode (F9)"
    send_key "$wid" F9 500
    sleep 2
    send_key "$wid" F9 500
    sleep 1

    echo "=> Test Complete."
    send_key "$wid" Escape 200
}
