import pytest


can_toggle = [
    'update_dragged_clients',
    ]

cannot_toggle = [
    'window_border_width',
    'frame_border_active_color',
    'default_frame_layout',
    'wmname']


@pytest.mark.parametrize('name', can_toggle)
def test_toggle_boolean_settings(hlwm, name):
    hlwm.call("toggle " + name)


@pytest.mark.parametrize('name', cannot_toggle)
def test_cannot_toggle_non_boolean(hlwm, name):
    p = hlwm.call_xfail("toggle " + name)
    assert p.stderr.endswith("not of type bool\n")


@pytest.mark.parametrize('name', can_toggle + cannot_toggle)
def test_get(hlwm, name):
    hlwm.call("get " + name)


@pytest.mark.parametrize('name', can_toggle)
def test_toggle_numeric_settings(hlwm, name):
    hlwm.call("toggle " + name)


@pytest.mark.parametrize('name', cannot_toggle)
def test_cannot_toggle_non_numeric(hlwm, name):
    hlwm.call_xfail("toggle " + name)


def test_toggle_completion(hlwm):
    res = hlwm.complete("toggle")
    for n in can_toggle:
        assert n in res
    for n in cannot_toggle:
        assert n not in res
