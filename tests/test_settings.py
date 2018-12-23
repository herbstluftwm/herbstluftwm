import pytest


@pytest.mark.parametrize('can_toggle',
    ['window_border_width',
    'update_dragged_clients',
    'default_frame_layout']
)
def test_toggle_numeric_settings(hlwm, can_toggle):
    hlwm.call("toggle " + can_toggle)


@pytest.mark.parametrize('cannot_toggle',
    ['frame_border_active_color',
    'wmname']
)
def test_cannot_toggle_non_numeric(hlwm, cannot_toggle):
    hlwm.call_xfail("toggle " + cannot_toggle)
