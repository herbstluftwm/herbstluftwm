import pytest


can_toggle = [
    'update_dragged_clients',
]

cannot_toggle = [
    'window_border_width',
    'frame_border_active_color',
    'default_frame_layout',
    'wmname'
]


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


def test_cycle_value_color(hlwm):
    values = ['#ff0000', '#00ff00', '#0000ff']
    # this setting is a color, and even a DynAttribute
    name = 'window_border_active_color'
    hlwm.call(f'set {name} orange')

    for i in range(0, 5):
        hlwm.call(['cycle_value', name] + values)
        assert hlwm.get_attr('settings.' + name) == values[i % len(values)]


def test_cycle_value_loop(hlwm):
    values = ['0', '1', '2', '2', '3', '4']
    name = 'frame_gap'
    hlwm.call(f'set {name} 3')
    # if we now run cycle_value multiple times, it should reach the 4
    # and in the next loop should hang at the 2
    expected_values = ['4', '0', '1', '2', '2', '2', '2', '2']
    for expected in expected_values:
        hlwm.call(['cycle_value', name] + values)
        assert hlwm.get_attr('settings.' + name) == expected
