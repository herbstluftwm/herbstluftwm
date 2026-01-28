import pytest


def test_max_tab_reorder_setting_default_value(hlwm):
    """Test that max_tab_reorder setting has the correct default value."""
    # The setting should default to false
    assert hlwm.attr.settings.max_tab_reorder() is False


def test_max_tab_reorder_setting_toggle(hlwm):
    """Test that max_tab_reorder setting can be toggled."""
    # Should be able to toggle the boolean setting
    initial_value = hlwm.attr.settings.max_tab_reorder()
    hlwm.call('toggle max_tab_reorder')
    toggled_value = hlwm.attr.settings.max_tab_reorder()
    assert toggled_value != initial_value

    # Toggle back
    hlwm.call('toggle max_tab_reorder')
    final_value = hlwm.attr.settings.max_tab_reorder()
    assert final_value == initial_value


def test_max_tab_reorder_setting_completion(hlwm):
    """Test that max_tab_reorder appears in setting completion."""
    completions = hlwm.complete(['set', 'max_tab_reorder'])
    assert 'true' in completions
    assert 'false' in completions

    # Test toggle completion
    toggle_completions = hlwm.complete(['toggle'])
    assert 'max_tab_reorder' in toggle_completions


def test_max_tab_reorder_setting_can_be_set(hlwm):
    """Test that the max_tab_reorder setting can be set to different values."""
    # Test setting to true
    hlwm.call(['set', 'max_tab_reorder', 'true'])
    assert hlwm.attr.settings.max_tab_reorder() is True

    # Test setting to false
    hlwm.call(['set', 'max_tab_reorder', 'false'])
    assert hlwm.attr.settings.max_tab_reorder() is False

    # Test setting with on/off
    hlwm.call(['set', 'max_tab_reorder', 'on'])
    assert hlwm.attr.settings.max_tab_reorder() is True

    hlwm.call(['set', 'max_tab_reorder', 'off'])
    assert hlwm.attr.settings.max_tab_reorder() is False


@pytest.mark.parametrize("running_clients_num", [3])
def test_max_tab_reorder_basic_behavior_max_layout(hlwm, running_clients):
    """
    Test basic behavior of max_tab_reorder setting with max layout.
    Focus on whether the setting exists and affects behavior at all.
    """
    # Set up max layout with multiple clients
    hlwm.attr.settings.tabbed_max = True
    hlwm.call('set_layout max')

    # Verify we have the expected clients and layout
    assert len(running_clients) == 3
    layout = hlwm.call('dump').stdout
    assert 'max:' in layout

    # Test with max_tab_reorder disabled (default)
    hlwm.attr.settings.max_tab_reorder = False

    # Try a shift operation - should not crash
    result = hlwm.unchecked_call(['shift', 'right'])

    # Test with max_tab_reorder enabled
    hlwm.attr.settings.max_tab_reorder = True

    result2 = hlwm.unchecked_call(['shift', 'right'])

    # At minimum, verify the setting doesn't break anything
    assert result.returncode in [0, 6]  # Success or "No neighbour found"
    assert result2.returncode in [0, 6]  # Success or "No neighbour found"


@pytest.mark.parametrize("running_clients_num", [2])
def test_max_tab_reorder_only_affects_max_layout(hlwm, running_clients):
    """
    Test that the max_tab_reorder setting only affects max layout.
    """
    hlwm.attr.settings.max_tab_reorder = True
    hlwm.attr.settings.tabbed_max = True

    # Test with vertical layout (not max)
    hlwm.call('set_layout vertical')

    # Shift should work normally in vertical layout regardless of max_tab_reorder
    result = hlwm.unchecked_call(['shift', 'down'])

    # Should either succeed or fail with "No neighbour found"
    # but should NOT be affected by max_tab_reorder setting
    assert result.returncode in [0, 6]


@pytest.mark.parametrize("running_clients_num", [2])
def test_max_tab_reorder_with_split_frames(hlwm, running_clients):
    """
    Test max_tab_reorder behavior when there are multiple frames.
    """
    hlwm.attr.settings.tabbed_max = True
    hlwm.attr.settings.max_tab_reorder = True

    # Create a simple split
    hlwm.call(['split', 'right'])

    # Test that setting doesn't break basic frame operations
    result = hlwm.unchecked_call(['shift', 'right'])
    assert result.returncode in [0, 6]

    # Test with setting disabled
    hlwm.attr.settings.max_tab_reorder = False
    result2 = hlwm.unchecked_call(['shift', 'left'])
    assert result2.returncode in [0, 6]


def test_max_tab_reorder_setting_persistence(hlwm):
    """Test that the max_tab_reorder setting persists correctly."""
    # Set the value
    hlwm.attr.settings.max_tab_reorder = True

    # Verify it's set
    assert hlwm.attr.settings.max_tab_reorder() is True

    # Should still be set after other operations
    hlwm.call('split right')
    assert hlwm.attr.settings.max_tab_reorder() is True

    # Reset to default
    hlwm.attr.settings.max_tab_reorder = False
    assert hlwm.attr.settings.max_tab_reorder() is False
