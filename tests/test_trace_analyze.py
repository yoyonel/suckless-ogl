"""
Unit tests for trace_analyze.py

Tests the core functionality of the ApiTrace analysis script by using
its exposed API functions directly.
"""

import sys
from io import StringIO
from unittest.mock import patch

# Add scripts directory to path
sys.path.insert(0, "scripts")
import trace_analyze


def test_get_shader_name_fixed_function():
    """Test shader name extraction for fixed function pipeline."""
    prog_labels = {1: "shaders/test.glsl"}

    result = trace_analyze.get_shader_name(0, prog_labels)
    assert result == "[Fixed Function / Clear]"


def test_get_shader_name_labeled_shader():
    """Test shader name extraction for labeled shader."""
    prog_labels = {42: "shaders/pbr.vert + shaders/pbr.frag"}

    result = trace_analyze.get_shader_name(42, prog_labels)
    assert result == "shaders/pbr.vert + shaders/pbr.frag"


def test_get_shader_name_unlabeled_fallback():
    """Test shader name extraction falls back to ID for unlabeled shaders."""
    prog_labels = {}

    result = trace_analyze.get_shader_name(99, prog_labels)
    assert result == "99"


def test_extract_timer_duration_valid_pair():
    """Test timer duration extraction with valid timestamp pair."""
    start_call = 100
    end_call = 200
    timestamp_fetches = [
        (201, 1000000000),  # Start: 1s
        (202, 1500000000),  # End: 1.5s
        (300, 2000000000),  # Unrelated
    ]

    duration, has_timer = trace_analyze.extract_timer_duration(
        start_call, end_call, timestamp_fetches
    )

    assert has_timer is True
    assert duration == 500000000  # 500ms in nanoseconds
    assert duration / 1e6 == 500.0


def test_extract_timer_duration_no_pair():
    """Test timer duration extraction when no valid pair exists."""
    start_call = 100
    end_call = 200
    timestamp_fetches = [
        (300, 1000000000),  # Too far away
    ]

    duration, has_timer = trace_analyze.extract_timer_duration(
        start_call, end_call, timestamp_fetches
    )

    assert has_timer is False
    assert duration == 0


def test_find_best_marker_selects_narrowest():
    """Test that find_best_marker selects the most specific marker."""
    call_id = 150
    marker_instances = [
        {"id": 0, "start": 100, "end": 500, "range": 400, "label": "Broad"},
        {"id": 1, "start": 140, "end": 160, "range": 20, "label": "Narrow"},
        {"id": 2, "start": 120, "end": 200, "range": 80, "label": "Medium"},
    ]

    best = trace_analyze.find_best_marker(call_id, marker_instances)

    assert best is not None
    assert best["label"] == "Narrow"
    assert best["range"] == 20


def test_find_best_marker_no_match():
    """Test find_best_marker returns None when no marker contains the call."""
    call_id = 50
    marker_instances = [
        {"id": 0, "start": 100, "end": 200, "range": 100, "label": "Test"},
    ]

    best = trace_analyze.find_best_marker(call_id, marker_instances)

    assert best is None


def test_create_marker_instances():
    """Test marker instance creation from raw markers and timestamps."""
    call_markers_raw = [
        (100, 200, "Test Group 1"),
        (300, 400, "Test Group 2"),
    ]
    timestamp_fetches = [
        (201, 1000000000),
        (202, 1500000000),  # Valid pair for first group
        (401, 2000000000),
        (402, 2300000000),  # Valid pair for second group
    ]

    instances = trace_analyze.create_marker_instances(
        call_markers_raw, timestamp_fetches
    )

    assert len(instances) == 2

    # First instance
    assert instances[0]["id"] == 0
    assert instances[0]["start"] == 100
    assert instances[0]["end"] == 200
    assert instances[0]["label"] == "Test Group 1"
    assert instances[0]["has_own_timer"] is True
    assert instances[0]["duration"] == 500000000  # 500ms

    # Second instance
    assert instances[1]["id"] == 1
    assert instances[1]["start"] == 300
    assert instances[1]["end"] == 400
    assert instances[1]["label"] == "Test Group 2"
    assert instances[1]["has_own_timer"] is True
    assert instances[1]["duration"] == 300000000  # 300ms


def test_calculate_nested_timer_sums():
    """Test nested timer sum calculation for parent groups."""
    marker_instances = [
        {
            "id": 0,
            "start": 100,
            "end": 500,
            "label": "Parent",
            "duration": 0,
            "has_own_timer": False,
            "range": 400,
        },
        {
            "id": 1,
            "start": 150,
            "end": 250,
            "label": "Child 1",
            "duration": 100000000,  # 100ms
            "has_own_timer": True,
            "range": 100,
        },
        {
            "id": 2,
            "start": 300,
            "end": 400,
            "label": "Child 2",
            "duration": 200000000,  # 200ms
            "has_own_timer": True,
            "range": 100,
        },
    ]

    trace_analyze.calculate_nested_timer_sums(marker_instances)

    # Parent should have sum of children
    assert marker_instances[0]["duration"] == 300000000  # 100ms + 200ms
    assert marker_instances[0]["is_sum"] is True

    # Children should not be marked as sums
    assert marker_instances[1]["is_sum"] is False
    assert marker_instances[2]["is_sum"] is False


def test_calculate_nested_timer_sums_indirect():
    """Test nested timer sum calculation skips indirect children."""
    marker_instances = [
        {
            "id": 0,
            "start": 100,
            "end": 600,
            "duration": 0,
            "has_own_timer": False,
            "range": 500,
            "label": "Parent",
        },
        {
            "id": 1,
            "start": 150,
            "end": 500,
            "duration": 400000000,
            "has_own_timer": True,
            "range": 350,
            "label": "Direct Child",
        },
        {
            "id": 2,
            "start": 200,
            "end": 400,
            "duration": 200000000,
            "has_own_timer": True,
            "range": 200,
            "label": "Indirect Child (Grandchild)",
        },
    ]

    trace_analyze.calculate_nested_timer_sums(marker_instances)

    # Parent should only sum direct child (400ms), NOT the grandchild
    assert marker_instances[0]["duration"] == 400000000
    assert marker_instances[0]["is_sum"] is True


def test_parse_trace_dump_labels():
    """Test parsing of glObjectLabel calls."""
    dump_lines = [
        '1234 glObjectLabel(identifier = GL_PROGRAM, name = 42, length = -1, label = "shaders/test.glsl")',
        '5678 glObjectLabel(identifier = GL_PROGRAM, name = 99, length = -1, label = "shaders/pbr.frag")',
    ]

    prog_labels, _, _ = trace_analyze.parse_trace_dump(iter(dump_lines))

    assert len(prog_labels) == 2
    assert prog_labels[42] == "shaders/test.glsl"
    assert prog_labels[99] == "shaders/pbr.frag"


def test_parse_trace_dump_debug_groups():
    """Test parsing of debug group push/pop."""
    dump_lines = [
        '100 glPushDebugGroup(source = GL_DEBUG_SOURCE_APPLICATION, id = 0, length = -1, message = "IBL Generation")',
        "200 glPopDebugGroup()",
        '300 glPushDebugGroup(source = GL_DEBUG_SOURCE_APPLICATION, id = 0, length = -1, message = "Rendering")',
        "400 glPopDebugGroup()",
    ]

    _, call_markers_raw, _ = trace_analyze.parse_trace_dump(iter(dump_lines))

    assert len(call_markers_raw) == 2
    assert call_markers_raw[0] == (100, 200, "IBL Generation")
    assert call_markers_raw[1] == (300, 400, "Rendering")


def test_parse_trace_dump_timestamp_fetches():
    """Test parsing of glGetQueryObjectui64v calls."""
    dump_lines = [
        "16038 glGetQueryObjectui64v(id = 3, pname = GL_QUERY_RESULT, params = &3550341052030)",
        "16039 glGetQueryObjectui64v(id = 4, pname = GL_QUERY_RESULT, params = &3550650788280)",
    ]

    _, _, timestamp_fetches = trace_analyze.parse_trace_dump(iter(dump_lines))

    assert len(timestamp_fetches) == 2
    assert timestamp_fetches[0] == (16038, 3550341052030)
    assert timestamp_fetches[1] == (16039, 3550650788280)


def test_print_shader_table_formatting():
    """Test shader table output formatting."""
    stats = {
        "shaders/test.glsl": {
            "draws": 100,
            "gpu": 50000000,  # 50ms
            "app_timer": 75000000,  # 75ms
            "seen_marks": set(),
        }
    }
    frame_count = 10

    output = StringIO()
    with patch("sys.stdout", output):
        trace_analyze.print_shader_table(stats, frame_count)

    result = output.getvalue()

    assert "Performance by Shader" in result
    assert "shaders/test.glsl" in result
    assert "100" in result  # draws
    assert "50.00" in result  # GPU ms
    assert "75.0" in result  # Timer ms


def test_print_instance_table_with_sum_indicator():
    """Test instance table shows asterisk for summed timers."""
    instances = [
        {
            "id": 0,
            "start": 100,
            "end": 200,
            "label": "Test Group",
            "duration": 500000000,  # 500ms
            "is_sum": True,
            "calls": 50,
            "gpu": 10000000,
            "range": 100,
        }
    ]

    output = StringIO()
    with patch("sys.stdout", output):
        trace_analyze.print_instance_table(instances)

    result = output.getvalue()

    assert "Debug Groups" in result
    assert "Test Group" in result
    assert "100-200" in result
    assert "500.0*" in result  # Asterisk for sum


def test_process_replay_output():
    """Test processing of apitrace replay output."""
    replay_lines = [
        "call 100 0 1 5000000 0",  # call no=100 program=1 gpu_dura=5000000
        "call 150 0 1 3000000 0",  # call no=150 program=1 gpu_dura=3000000
        "call 200 0 1 2000000 0",  # call no=200 program=1 gpu_dura=2000000
    ]
    fields = ["no", "ignore", "program", "gpu_dura", "ignore2"]
    prog_labels = {1: "shaders/test.glsl"}
    marker_instances = [
        {
            "id": 0,
            "start": 50,
            "end": 250,
            "label": "IBL: EnvMap",
            "duration": 10000000,
            "has_own_timer": True,
            "range": 200,
            "calls": 0,
            "gpu": 0,
        }
    ]

    stats = trace_analyze.process_replay_output(
        iter(replay_lines), fields, prog_labels, marker_instances
    )

    assert "shaders/test.glsl" in stats
    assert stats["shaders/test.glsl"]["draws"] == 3
    assert stats["shaders/test.glsl"]["gpu"] == 10000000  # 5 + 3 + 2

    # Verify IBL timer attribution
    assert stats["shaders/test.glsl"]["app_timer"] == 10000000

    # Verify marker updates
    assert marker_instances[0]["calls"] == 3
    assert marker_instances[0]["gpu"] == 10000000


def test_get_frame_count_success():
    """Test frame count extraction success."""
    apitrace_bin = "apitrace"
    trace_file = "test.trace"
    mock_json = '{"FramesCount": 42}'

    with patch("subprocess.check_output", return_value=mock_json):
        result = trace_analyze.get_frame_count(apitrace_bin, trace_file)
        assert result == 42


def test_get_frame_count_failure():
    """Test frame count extraction failure returns default 1."""
    apitrace_bin = "apitrace"
    trace_file = "test.trace"

    with patch(
        "subprocess.check_output", side_effect=trace_analyze.subprocess.SubprocessError
    ):
        result = trace_analyze.get_frame_count(apitrace_bin, trace_file)
        assert result == 1


def test_main_minimal():
    """Test main function with a minimal simulated trace."""
    from unittest.mock import MagicMock

    with (
        patch("trace_analyze.get_frame_count", return_value=1),
        patch("subprocess.Popen") as mock_popen,
        patch("trace_analyze.print_shader_table"),
        patch("trace_analyze.print_instance_table"),
        patch("sys.argv", ["trace_analyze.py", "test.trace"]),
    ):
        # Mock for dump process
        mock_dump = MagicMock()
        mock_dump.stdout = StringIO(
            '10 glObjectLabel(identifier = GL_PROGRAM, name = 1, length = -1, label = "shaders/test.glsl")\n'
            '20 glPushDebugGroup(source = GL_DEBUG_SOURCE_APPLICATION, id = 0, length = -1, message = "Group")\n'
            "30 glPopDebugGroup()\n"
            "31 glGetQueryObjectui64v(id = 1, pname = GL_QUERY_RESULT, params = &1000)\n"
            "32 glGetQueryObjectui64v(id = 2, pname = GL_QUERY_RESULT, params = &2000)\n"
        )
        mock_dump.wait.return_value = 0

        # Setup second call to Popen (replay)
        mock_replay = MagicMock()
        mock_replay.stdout = StringIO("# no program gpu_dura\ncall 25 0 1 5000\n")
        mock_replay.wait.return_value = 0

        mock_popen.side_effect = [mock_dump, mock_replay]

        trace_analyze.main()


if __name__ == "__main__":
    # Try to run with pytest for better output
    import subprocess
    import shutil

    if shutil.which("pytest"):
        # pytest is available as executable
        try:
            subprocess.run(["pytest", __file__, "-v"], check=True)
        except subprocess.CalledProcessError:
            exit(1)
    else:
        # Fallback to basic assertions
        print("Running basic test assertions (install pytest for better output)...")
        test_get_shader_name_fixed_function()
        test_get_shader_name_labeled_shader()
        test_get_shader_name_unlabeled_fallback()
        test_extract_timer_duration_valid_pair()
        test_extract_timer_duration_no_pair()
        test_find_best_marker_selects_narrowest()
        test_find_best_marker_no_match()
        test_create_marker_instances()
        test_calculate_nested_timer_sums()
        test_calculate_nested_timer_sums_indirect()
        test_parse_trace_dump_labels()
        test_parse_trace_dump_debug_groups()
        test_parse_trace_dump_timestamp_fetches()
        test_print_shader_table_formatting()
        test_print_instance_table_with_sum_indicator()
        test_process_replay_output()
        test_get_frame_count_success()
        test_get_frame_count_failure()
        test_main_minimal()
        print("✓ All 19 tests passed!")
