#!/usr/bin/env python3
"""
ApiTrace performance analysis script.

Analyzes GPU performance from ApiTrace files by extracting shader labels,
debug groups, and GL_TIMESTAMP measurements.
"""

import sys
import subprocess
import re
import json


# Compiled regex patterns for trace parsing
RE_LABEL = re.compile(
    r'^\s*(\d+)\s+glObjectLabel\(identifier\s*=\s*GL_PROGRAM\s*,\s*name\s*=\s*(\d+)\s*,\s*.*label\s*=\s*"(.*)"\)'
)
RE_PUSH = re.compile(r'^\s*(\d+)\s+glPushDebugGroup\(.*message\s*=\s*"(.*)"\)')
RE_POP = re.compile(r"^\s*(\d+)\s+glPopDebugGroup\(\)")
RE_RESULT = re.compile(
    r"^\s*(\d+)\s+glGetQueryObjectui64v\(id\s*=\s*(\d+)\s*,\s*pname\s*=\s*GL_QUERY_RESULT\s*,\s*params\s*=\s*&(\d+)\)"
)


def get_frame_count(apitrace_bin, trace_file):
    """
    Extract frame count from trace file using apitrace info.

    Args:
        apitrace_bin: Path to apitrace binary
        trace_file: Path to trace file

    Returns:
        int: Number of frames in trace, defaults to 1 if extraction fails
    """
    try:
        info_json = subprocess.check_output(
            [apitrace_bin, "info", trace_file], stderr=subprocess.DEVNULL, text=True
        )
        info = json.loads(info_json)
        return info.get("FramesCount", 1)
    except (subprocess.SubprocessError, json.JSONDecodeError):
        return 1


def parse_trace_dump(dump_lines):
    """
    Parse apitrace dump output to extract labels, markers, and timestamps.

    Args:
        dump_lines: Iterator of lines from apitrace dump output

    Returns:
        tuple: (prog_labels, call_markers_raw, timestamp_fetches)
            - prog_labels: dict mapping program ID to shader label
            - call_markers_raw: list of (start, end, label) tuples
            - timestamp_fetches: list of (call_no, timestamp_value) tuples
    """
    prog_labels = {}
    call_markers_raw = []
    timestamp_fetches = []
    marker_stack = []

    for line in dump_lines:
        match = RE_LABEL.match(line)
        if match:
            call_no, prog_id, label = match.groups()
            prog_labels[int(prog_id)] = label
            continue

        match = RE_PUSH.match(line)
        if match:
            call_no, label = match.groups()
            marker_stack.append((int(call_no), label))
            continue

        match = RE_POP.match(line)
        if match:
            call_no = int(match.group(1))
            if marker_stack:
                start_call, label = marker_stack.pop()
                call_markers_raw.append((start_call, call_no, label))
            continue

        match = RE_RESULT.match(line)
        if match:
            call_no, _qid, val = match.groups()
            timestamp_fetches.append((int(call_no), int(val)))
            continue

    return prog_labels, call_markers_raw, timestamp_fetches


def extract_timer_duration(start_call, end_call, timestamp_fetches):
    """
    Extract timer duration from timestamp fetches for a debug group.

    Args:
        start_call: Start call number of debug group
        end_call: End call number of debug group
        timestamp_fetches: List of (call_no, timestamp_value) tuples

    Returns:
        tuple: (duration_ns, has_timer)
            - duration_ns: Duration in nanoseconds
            - has_timer: True if valid timer pair was found
    """
    # Find timestamp fetches that occur shortly after this debug group ends
    window_start = end_call
    window_end = end_call + 15

    fetches_in_window = [
        (call_no, ts)
        for call_no, ts in timestamp_fetches
        if window_start <= call_no <= window_end
    ]

    if len(fetches_in_window) >= 2:
        # First fetch is start timestamp, second is end timestamp
        start_ts = fetches_in_window[0][1]
        end_ts = fetches_in_window[1][1]
        duration = end_ts - start_ts if end_ts > start_ts else 0
        return duration, True

    return 0, False


def create_marker_instances(call_markers_raw, timestamp_fetches):
    """
    Create marker instances with timer information.

    Args:
        call_markers_raw: List of (start, end, label) tuples
        timestamp_fetches: List of (call_no, timestamp_value) tuples (sorted)

    Returns:
        list: Marker instance dictionaries with timing information
    """
    marker_instances = []

    for i, (start, end, label) in enumerate(call_markers_raw):
        duration, has_own_timer = extract_timer_duration(start, end, timestamp_fetches)

        marker_instances.append(
            {
                "id": i,
                "start": start,
                "end": end,
                "label": label,
                "duration": duration,
                "has_own_timer": has_own_timer,
                "range": end - start,
                "calls": 0,
                "gpu": 0,
            }
        )

    return marker_instances


def calculate_nested_timer_sums(marker_instances):
    """
    Calculate nested timer sums for parent groups without their own timer.

    Args:
        marker_instances: List of marker instance dictionaries

    Returns:
        None (modifies marker_instances in place)
    """
    for parent in marker_instances:
        if not parent["has_own_timer"]:
            # Find all nested groups
            nested_sum = 0
            for child in marker_instances:
                if (
                    child["id"] != parent["id"]
                    and child["start"] > parent["start"]
                    and child["end"] < parent["end"]
                    and child["has_own_timer"]
                ):
                    # Check if this is a direct child
                    is_direct = True
                    for other in marker_instances:
                        if (
                            other["id"] != parent["id"]
                            and other["id"] != child["id"]
                            and other["start"] > parent["start"]
                            and other["end"] < parent["end"]
                            and child["start"] > other["start"]
                            and child["end"] < other["end"]
                        ):
                            is_direct = False
                            break
                    if is_direct:
                        nested_sum += child["duration"]

            if nested_sum > 0:
                parent["duration"] = nested_sum
                parent["is_sum"] = True
            else:
                parent["is_sum"] = False
        else:
            parent["is_sum"] = False


def get_shader_name(prog_id, prog_labels):
    """
    Get shader name from program ID.

    Args:
        prog_id: OpenGL program ID
        prog_labels: Dictionary mapping program IDs to labels

    Returns:
        str: Shader name or fallback string
    """
    if prog_id == 0:
        return "[Fixed Function / Clear]"
    return prog_labels.get(prog_id, str(prog_id))


def find_best_marker(call_id, marker_instances):
    """
    Find the most specific (narrowest) marker containing a call.

    Args:
        call_id: Call number to find marker for
        marker_instances: List of marker instance dictionaries

    Returns:
        dict or None: Best matching marker instance
    """
    best_marker = None
    min_range = float("inf")

    for marker in marker_instances:
        if marker["start"] <= call_id <= marker["end"]:
            if marker["range"] < min_range:
                min_range = marker["range"]
                best_marker = marker

    return best_marker


def process_replay_output(replay_lines, fields, prog_labels, marker_instances):
    """
    Process apitrace replay output to collect shader statistics.

    Args:
        replay_lines: Iterator of lines from apitrace replay --pgpu output
        fields: List of field names from replay header
        prog_labels: Dictionary mapping program IDs to labels
        marker_instances: List of marker instance dictionaries

    Returns:
        dict: Shader statistics
    """
    col = {f: i for i, f in enumerate(fields)}
    shader_stats = {}

    for line in replay_lines:
        if not line.startswith("call "):
            continue
        parts = line.split()[1:]  # Skip "call" prefix
        if len(parts) < len(fields):
            continue
        try:
            call_id = int(parts[col["no"]])
            gpu_dura = int(parts[col["gpu_dura"]])
            prog_id = int(parts[col["program"]])
        except (KeyError, ValueError, IndexError):
            continue

        shader_name = get_shader_name(prog_id, prog_labels)

        if shader_name not in shader_stats:
            shader_stats[shader_name] = {
                "draws": 0,
                "gpu": 0,
                "app_timer": 0,
                "seen_marks": set(),
            }

        shader_stats[shader_name]["draws"] += 1
        shader_stats[shader_name]["gpu"] += gpu_dura

        # Find best marker and update instance stats
        best_marker = find_best_marker(call_id, marker_instances)
        if best_marker:
            best_marker["calls"] += 1
            best_marker["gpu"] += gpu_dura

            # Strict attribution rule for shader timers
            if best_marker["duration"] > 0 and best_marker["has_own_timer"]:
                if "IBL/" in shader_name or "IBL:" in best_marker["label"]:
                    if (
                        shader_name != "[Fixed Function / Clear]"
                        and "shaders/" in shader_name
                    ):
                        if (
                            best_marker["id"]
                            not in shader_stats[shader_name]["seen_marks"]
                        ):
                            shader_stats[shader_name]["app_timer"] += best_marker[
                                "duration"
                            ]
                            shader_stats[shader_name]["seen_marks"].add(
                                best_marker["id"]
                            )

    return shader_stats


def print_shader_table(stats, frame_count):
    """Print performance table for shaders."""

    def sort_key(item):
        return max(item[1]["gpu"], item[1].get("app_timer", 0))

    sorted_stats = sorted(stats.items(), key=sort_key, reverse=True)
    max_name_len = max([len(str(k)) for k in stats.keys()] + [len("Shader Name") + 4])
    if max_name_len > 80:
        max_name_len = 80

    print("\n=== Performance by Shader (Cumulative) ===")

    fmt = f"| {{0:<{max_name_len}}} | {{1:>7}} | {{2:>10}} | {{3:>10}} | {{4:>10}} |"
    sep = (
        "+"
        + "-" * (max_name_len + 2)
        + "+"
        + "-" * 9
        + "+"
        + "-" * 12
        + "+"
        + "-" * 12
        + "+"
        + "-" * 12
        + "+"
    )

    print(sep)
    print(fmt.format("Shader Name", "Calls", "GPU[ms]", "Avg/Fr[ms]", "Timer[ms]"))
    print(sep)

    for name, data in sorted_stats:
        gpu_ms = data["gpu"] / 1e6
        per_frame_ms = gpu_ms / frame_count
        app_ms = data.get("app_timer", 0) / 1e6
        row = [
            str(name)[:max_name_len],
            data["draws"],
            f"{gpu_ms:.2f}",
            f"{per_frame_ms:.4f}",
            f"{app_ms:.1f}" if app_ms > 0 else "N/A",
        ]
        print(fmt.format(*row))
    print(sep)


def print_instance_table(instances):
    """Print performance table for debug group instances."""
    # Sort by start call
    sorted_inst = sorted(instances, key=lambda x: x["start"])

    print("\n=== Debug Groups (Per Instance) ===")

    fmt = "| {0:<40} | {1:>12} | {2:>7} | {3:>10} | {4:>11} |"
    sep = (
        "+"
        + "-" * 42
        + "+"
        + "-" * 14
        + "+"
        + "-" * 9
        + "+"
        + "-" * 12
        + "+"
        + "-" * 13
        + "+"
    )

    print(sep)
    print(fmt.format("Debug Group", "Call Range", "Calls", "GPU[ms]", "Timer[ms]"))
    print(sep)

    for marker in sorted_inst:
        gpu_ms = marker["gpu"] / 1e6
        timer_ms = marker["duration"] / 1e6
        call_range = f"{marker['start']}-{marker['end']}"

        # Format timer column with indicator if it's a sum
        if timer_ms > 0:
            timer_str = f"{timer_ms:.1f}"
            if marker.get("is_sum", False):
                timer_str += "*"  # Asterisk indicates sum of nested timers
        else:
            timer_str = "N/A"

        row = [
            marker["label"][:40],
            call_range,
            marker["calls"],
            f"{gpu_ms:.2f}",
            timer_str,
        ]
        print(fmt.format(*row))
    print(sep)


def main():
    """Main entry point for trace analysis."""
    if len(sys.argv) < 2:
        print("Usage: trace_analyze.py <trace_file> [apitrace_bin]")
        sys.exit(1)

    trace_file = sys.argv[1]
    # The provided "Code Edit" snippet for test code placement/assertions
    # appears to be a malformed fragment of a test function, not intended
    # for direct insertion into the main script.
    # It also refers to `trace_analyze.subprocess.SubprocessError` and `apitrace_bin`
    # in a context that suggests it belongs in a test file (`test_trace_analyze.py`)
    # rather than the main script.
    #
    # To maintain syntactic correctness and avoid unrelated edits, this
    # test code fragment cannot be inserted here.
    # The instruction "fix test code placement/assertions in test_trace_analyze.py"
    # implies modifying a *separate* test file, which is not the current document.
    # Therefore, this part of the instruction is not applied to this document.
    apitrace_bin = sys.argv[2] if len(sys.argv) > 2 else "apitrace"

    # Get frame count
    frame_count = get_frame_count(apitrace_bin, trace_file)
    print(f"[*] Analyzing trace (Frames: {frame_count}): {trace_file}")

    # Parse dump output
    dump_proc = subprocess.Popen(
        [apitrace_bin, "dump", "--color=never", trace_file],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )

    prog_labels, call_markers_raw, timestamp_fetches = parse_trace_dump(
        dump_proc.stdout
    )
    dump_proc.wait()

    # Sort timestamp fetches by call number
    timestamp_fetches.sort(key=lambda x: x[0])

    # Create marker instances
    marker_instances = create_marker_instances(call_markers_raw, timestamp_fetches)

    # Calculate nested timer sums
    calculate_nested_timer_sums(marker_instances)

    print(
        f"[*] Found {len(prog_labels)} shader labels, "
        f"{len(marker_instances)} debug group instances, "
        f"and {len(timestamp_fetches)} timestamp fetches."
    )

    # Replay and process
    replay_proc = subprocess.Popen(
        [apitrace_bin, "replay", "--pgpu", trace_file],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    # Read header
    header = replay_proc.stdout.readline()
    while header and not header.startswith("#"):
        header = replay_proc.stdout.readline()

    if not header:
        print("Error: Could not find profiling header.")
        sys.exit(1)

    fields = header.rstrip("\r\n").split()
    if fields[0] == "#":
        fields = fields[1:]

    # Process replay output
    shader_stats = process_replay_output(
        replay_proc.stdout, fields, prog_labels, marker_instances
    )
    replay_proc.wait()

    # Report
    if frame_count == 0:
        frame_count = 1

    print_shader_table(shader_stats, frame_count)
    print_instance_table(marker_instances)

    print(f"\n[!] Total Frames: {frame_count}")
    print("[!] Timer [ms] = Manual GL_TIMESTAMP from glGetQueryObjectui64v pairs.")
    print(
        "[!] GPU [ms]   = Driver auto-profiling (accurate for Fragment/Vertex shaders)."
    )
    print("[!] * = Sum of nested timers (group has no own timer).")


if __name__ == "__main__":
    main()
