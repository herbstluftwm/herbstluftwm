import pytest


@pytest.mark.parametrize('can_toggle',
    ['update_dragged_clients']
)
def test_toggle_boolean_settings(hlwm, can_toggle):
    hlwm.call("toggle " + can_toggle)


@pytest.mark.parametrize('cannot_toggle',
    ['window_border_width',
    'frame_border_active_color',
    'default_frame_layout',
    'wmname']
)
def test_cannot_toggle_non_boolean(hlwm, cannot_toggle):
    p = hlwm.call_xfail("toggle " + cannot_toggle)
    assert p.stderr.endswith("not of type bool\n")
