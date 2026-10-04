"""Spatial entry into neighbouring subtrees; run with hlwm's Xvfb fixtures."""
import pytest


@pytest.fixture(autouse=True)
def spatial_settings(hlwm):
    for setting in ['frame_gap', 'frame_border_width', 'frame_padding', 'window_gap']:
        hlwm.call(['set', setting, '0'])
    hlwm.call('set default_direction_external_only off')
    hlwm.call('set focus_crosses_monitor_boundaries off')
    hlwm.call('set focus_follows_mouse off')


@pytest.mark.parametrize('algorithm,direction,path', [
    ('horizontal', 'right', [0, 1, 2, 3]),
    ('horizontal', 'left', [3, 2, 1, 0]),
    ('vertical', 'down', [0, 1, 2, 3]),
    ('vertical', 'up', [3, 2, 1, 0]),
])
@pytest.mark.parametrize('flags', [[], ['-i'], ['--level=tabs'], ['--level=all']])
def test_spatial_linear(hlwm, algorithm, direction, path, flags):
    w = hlwm.create_clients(4)
    hlwm.call(['load', f'''(split {algorithm}:0.5:0
        (clients {algorithm}:0 {w[0]} {w[1]})
        (clients {algorithm}:1 {w[2]} {w[3]}))'''])
    hlwm.call(['jumpto', w[path[-1]]])
    hlwm.call(['jumpto', w[path[0]]])
    for index in path[1:]:
        hlwm.call(['focus'] + flags + [direction])
        assert hlwm.attr.clients.focus.winid() == w[index]
    hlwm.call_xfail(['focus'] + flags + [direction]).expect_stderr('No neighbour')


def tiles(w, nested):
    # Both variants arrange clients as [0 1 / 2 3], with different remembered focus.
    if not nested:
        return f'(clients grid:3 {" ".join(w)})'
    return f'''(split vertical:0.5:0
        (clients horizontal:1 {w[0]} {w[1]})
        (split horizontal:0.5:1
            (clients max:0 {w[2]}) (clients max:0 {w[3]})))'''


@pytest.mark.parametrize('align', ['horizontal', 'vertical'])
@pytest.mark.parametrize('nested_a,nested_b', [(False, False), (False, True),
                                               (True, False), (True, True)])
@pytest.mark.parametrize('gap', [0, 9])
def test_spatial_grid_and_nested(hlwm, align, nested_a, nested_b, gap):
    hlwm.call(['set', 'frame_gap', str(gap)])
    hlwm.call(['set', 'window_gap', str(gap)])
    w = hlwm.create_clients(8)
    layout = f'(split {align}:0.4:0 {tiles(w[:4], nested_a)} {tiles(w[4:], nested_b)})'
    transitions = ([(1, 'right', 4), (3, 'right', 6), (4, 'left', 1), (6, 'left', 3)]
                   if align == 'horizontal' else
                   [(2, 'down', 4), (3, 'down', 5), (4, 'up', 2), (5, 'up', 3)])
    for source, direction, target in transitions:
        hlwm.call(['load', layout])
        hlwm.call(['jumpto', w[source]])
        hlwm.call(['focus', '-i', direction])
        assert hlwm.attr.clients.focus.winid() == w[target]


@pytest.mark.parametrize('rotations,direction', [(0, 'right'), (1, 'up'),
                                                 (2, 'left'), (3, 'down')])
def test_spatial_tall_near_tile_beats_far_centre(hlwm, rotations, direction):
    # The near tile's centre lies outside the floating algorithm's 45-degree cone.
    a, b, near, far = hlwm.create_clients(4)
    hlwm.call(['load', f'''(split horizontal:0.1:0
        (split vertical:0.75:1 (clients max:0 {a}) (clients max:0 {b}))
        (split horizontal:0.111:1 (clients max:0 {near}) (clients max:0 {far})))'''])
    for _ in range(rotations):
        hlwm.call('rotate')
    hlwm.call(['focus', '-i', direction])
    assert hlwm.attr.clients.focus.winid() == near


def test_spatial_empty_leaf_is_not_skipped(hlwm):
    a, b, c = hlwm.create_clients(3)
    hlwm.call(['load', f'''(split horizontal:0.5:0
        (clients vertical:1 {a} {b})
        (split vertical:0.5:0 (clients max:0 {c}) (clients max:0)))'''])
    hlwm.call('focus -i right')
    assert hlwm.attr.tags.focus.tiling.focused_frame.index() == '11'
    assert 'focus' not in hlwm.list_children('clients')
    hlwm.call('focus -i left')
    assert hlwm.attr.clients.focus.winid() == b


@pytest.mark.parametrize('hide_covered', [False, True])
@pytest.mark.parametrize('pseudotile', [False, True])
@pytest.mark.parametrize('tabbed', [False, True])
def test_spatial_max_keeps_top_client(hlwm, hide_covered, pseudotile, tabbed):
    a, b, c = hlwm.create_clients(3)
    hlwm.attr.settings.hide_covered_windows = hide_covered
    hlwm.attr.settings.tabbed_max = tabbed
    hlwm.attr.clients[c].pseudotile = pseudotile
    layout = f'(split horizontal:0.5:0 (clients max:0 {a}) (clients max:1 {b} {c}))'
    for level in ['visible', 'tabs', 'all']:
        hlwm.call(['load', layout])
        hlwm.call(['focus', f'--level={level}', 'right'])
        assert hlwm.attr.clients.focus.winid() == c


@pytest.mark.parametrize('flag', [None, '-e', '--level=frame'])
def test_spatial_explicit_frame_focus_keeps_history(hlwm, flag):
    w = hlwm.create_clients(8)
    layout = f'(split horizontal:0.5:0 {tiles(w[:4], True)} {tiles(w[4:], True)})'
    hlwm.call(['load', layout])
    hlwm.call(['jumpto', w[7]])
    hlwm.call(['jumpto', w[1]])
    if flag is None:
        hlwm.call('set default_direction_external_only on')
    hlwm.call(['focus'] + ([flag] if flag else []) + ['right'])
    assert hlwm.attr.clients.focus.winid() == w[7]


def test_spatial_locked_load_uses_new_geometry(hlwm):
    a, b, c, d = hlwm.create_clients(4)
    hlwm.call('lock')
    try:
        hlwm.call(['load', f'''(split horizontal:0.5:0
            (clients horizontal:0 {a} {b}) (clients horizontal:1 {c} {d}))'''])
        hlwm.call('focus -i right')
        hlwm.call('focus -i right')
    finally:
        hlwm.call('unlock')
    assert hlwm.attr.clients.focus.winid() == c


@pytest.mark.parametrize('state', ['pseudotile', 'fullscreen'])
def test_spatial_uses_tile_slot_not_window_size(hlwm, state):
    a, b, c, d = hlwm.create_clients(4)
    hlwm.call(['load', f'''(split horizontal:0.5:0
        (clients horizontal:1 {a} {b}) (clients horizontal:1 {c} {d}))'''])
    hlwm.call(['set_attr', f'clients.{b}.{state}', 'true'])
    hlwm.call('focus -i right')
    assert hlwm.attr.clients.focus.winid() == c


@pytest.mark.parametrize('minimized', [False, True])
def test_spatial_does_not_enter_floating_layer(hlwm, minimized):
    a, b, c, d, floating = hlwm.create_clients(5)
    hlwm.attr.clients[floating].floating = True
    hlwm.attr.clients[floating].minimized = minimized
    hlwm.call(['load', f'''(split horizontal:0.5:0
        (clients horizontal:1 {a} {b}) (clients horizontal:1 {c} {d}))'''])
    hlwm.call('focus -i right')
    assert hlwm.attr.clients.focus.winid() == c


@pytest.mark.parametrize('gapless', [False, True])
def test_spatial_ragged_grid(hlwm, gapless):
    w = hlwm.create_clients(8)
    hlwm.attr.settings.gapless_grid = gapless
    hlwm.call(['load', f'''(split vertical:0.75:1
        (clients grid:0 {" ".join(w[:5])})
        (clients horizontal:2 {" ".join(w[5:])}))'''])
    hlwm.call('focus -i up')
    assert hlwm.attr.clients.focus.winid() == w[4 if gapless else 2]


@pytest.mark.parametrize('remembered', [0, 1])
def test_spatial_exact_tie_does_not_depend_on_history(hlwm, remembered):
    a, b, c = hlwm.create_clients(3)
    hlwm.call(['load', f'''(split horizontal:0.5:0
        (clients max:0 {a}) (clients vertical:{remembered} {b} {c}))'''])
    hlwm.call('focus -i right')
    assert hlwm.attr.clients.focus.winid() == b
