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


def test_default_frame_layout_value_too_high(hlwm):
    hlwm.call_xfail('set default_frame_layout 99') \
        .expect_stderr('set: Invalid value "99" for setting "default_frame_layout": .*out of range')


def test_default_frame_layout_value_invalid_value(hlwm):
    hlwm.call_xfail('set default_frame_layout -23') \
        .expect_stderr('set: Invalid value "-23" for setting "default_frame_layout": .*Expecting.*vertical')
    hlwm.call_xfail('set default_frame_layout foobar') \
        .expect_stderr('set: Invalid value "foobar" for setting "default_frame_layout": .*Expecting.*vertical')


def test_default_frame_layout_after_split(hlwm):
    """When splitting a FrameLeaf, then the new frame
    inherits the layout algorithm. However, when a FrameSplit is
    split, then default_frame_layout is used.
    """
    old_default = hlwm.attr.settings.default_frame_layout()
    new_default = 'grid'
    assert old_default != new_default, \
        "the test is vacuous if the default didn't change"
    hlwm.attr.settings.default_frame_layout = new_default
    hlwm.call('split right')

    # split the root frame
    hlwm.call(['split', 'bottom', '0.5', ''])

    # this new frame has the new default frame layout, but the two frames
    # on the top still have the original algorithm:
    assert hlwm.attr.tags.focus.tiling.root[0][0].algorithm() == old_default
    assert hlwm.attr.tags.focus.tiling.root[0][1].algorithm() == old_default
    assert hlwm.attr.tags.focus.tiling.root[1].algorithm() == new_default


def test_default_frame_layout_on_new_tag(hlwm):
    old_default = hlwm.attr.settings.default_frame_layout()
    new_default = 'grid'
    assert old_default != new_default, \
        "the test is vacuous if the default didn't change"
    hlwm.attr.settings.default_frame_layout = new_default

    hlwm.call('add newtag')

    assert hlwm.attr.tags[1].tiling.root.algorithm() == new_default
    assert hlwm.attr.tags[0].tiling.root.algorithm() == old_default


def test_default_frame_layout_index_as_name(hlwm):
    """test backwards compatibility of default_frame_layout"""
    layout_with_index_1 = 'horizontal'
    assert hlwm.attr.settings.default_frame_layout() != layout_with_index_1

    hlwm.attr.settings.default_frame_layout = '1'

    assert hlwm.attr.settings.default_frame_layout() == layout_with_index_1


def test_default_frame_layout_completion(hlwm):
    assert 'grid' in hlwm.complete(['set', 'default_frame_layout'])


def test_set_invalid_setting(hlwm):
    hlwm.call_xfail('set foobar baz') \
        .expect_stderr('Setting "foobar" not found\n')


def test_get_invalid_setting(hlwm):
    hlwm.call_xfail('get foobar') \
        .expect_stderr('Setting "foobar" not found\n')


def test_toggle_invalid_setting(hlwm):
    hlwm.call_xfail('toggle foobar') \
        .expect_stderr('Setting "foobar" not found\n')


def test_cycle_value_invalid_setting(hlwm):
    hlwm.call_xfail('cycle_value foobar baz') \
        .expect_stderr('Setting "foobar" not found\n')


def test_monitors_locked_negative_value(hlwm):
    hlwm.call_xfail('set monitors_locked -1') \
        .expect_stderr('out of range')
