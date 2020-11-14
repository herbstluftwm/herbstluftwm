import pytest
from test_layout import verify_frame_objects_via_dump


@pytest.mark.parametrize("invalid_layout,error_pos", [
    ('(', 1),
    ('()', 1),
    ('foo baar', 0),
    ('(foo baar', 1),
    ('((clients max:0 ))', 1),
    ('(clients)', 8),
    ('(clients )', 9),
    ('(split max:0.5:1)', 7),
    ('(split horizontal:0.05:1)', 7),
    ('(split horizontal:0.95:1)', 7),
    ('(split horizontal:x:1)', 7),
    ('(split horizontal:0.5:x)', 7),
    ('(split horizontal:0.5:-1)', 7),
    ('(split horizontal:0.5:2)', 7),
    ('(split horizontal:0.3)', 7),
    ('(split horizontal:0.3:0:0)', 7),
    ('(split  horizonta:0.5:0 )', 8),
    ('(clients max )', 9),
    ('(clients max:0:0 )', 9),
    ('(clients ma:0 )', 9),
    ('(clients max:-1 )', 9),
    ('(clients grid:0 asdf )', 16),
    ('(clients grid:0 0xx0)', 16),
    ('(clients grid:0 09)', 16),
    ('(clients grid:0 0x)', 16),
    ('(clients grid:0 x)', 16),
    ('(split horizontal:0.5:0 x)', 24),
    ('(split horizontal:0.5:0 (split horizontal:0.5:1', 47),
    ('(split horizontal:0.5:0 (split horizontal:0.5:1 ', 48),
    ('(split horizontal:0.5:0 (split horizontal:0.5:1 )', 49),
    ('(split horizontal:0.5:0 (split horizontal:0.5:1 )))', 50),
    ('(split horizontal:0.5:0 (clients max:1', 38),
])
def test_syntax_errors_position(hlwm, invalid_layout, error_pos):
    c = hlwm.call_xfail(['load', invalid_layout])
    c.expect_stderr(r'^load: Syntax error at {}: '.format(error_pos))


def is_subseq(x, y):
    """Checks if x is a subsequence (not substring) of y."""
    # from https://stackoverflow.com/a/24017747/4400896
    it = iter(y)
    return all(c in it for c in x)


@pytest.mark.parametrize("layout", [
    "(clients max:0)",
    "(clients grid:0)",
    " (  clients   vertical:0  )",
    "(split horizontal:0.3:0)",
    "(split vertical:0.3:0 (clients horizontal:0))",
    "(split vertical:0.3:0 (split vertical:0.4:1))",
])
@pytest.mark.parametrize('num_splits_before', [0, 1, 2])
def test_valid_layout_syntax_partial_layouts(hlwm, layout, num_splits_before):
    for i in range(0, num_splits_before):
        hlwm.call('split explode')

    # load the layout that defines the layout tree only partially
    hlwm.call(['load', layout])

    # The new layout is the old layout with some '(clients …)' (and theoretically
    # even '(split…)') subtrees inserted.
    assert is_subseq(layout.replace(' ', ''), hlwm.call('dump').stdout)


@pytest.mark.parametrize(
    "layout", [
        # with window ID placeholders 'W'
        "(clients max:0 W)",
        "(clients max:1 W W)",
        "(split horizontal:0.9:0 (split vertical:0.5:1 (clients max:0) (clients grid:0)) (clients horizontal:0))",
        "(split vertical:0.4:1 (clients max:2 W W W) (clients grid:0 W))",
    ])
def test_full_layouts(hlwm, layout):
    clients = [hlwm.create_client() for k in range(0, layout.count('W'))]
    for winid, _ in clients:
        # replace the next W by the window ID
        layout = layout.replace('W', winid, 1)

    p = hlwm.call(['load', layout])

    assert p.stdout == ''
    assert layout == hlwm.call('dump').stdout
    verify_frame_objects_via_dump(hlwm)


@pytest.mark.parametrize("layout", [
    "(clients horizontal:0 0234)",
    "(clients vertical:0 0x2343)",
    "(clients vertical:0 1713)",
])
def test_load_invalid_winids(hlwm, layout):
    p = hlwm.call(['load', layout])

    assert p.stdout.startswith("Warning: Unknown window IDs")


@pytest.mark.parametrize(
    "running_clients_num,focus",
    [(n, f) for n in [1, 3] for f in range(0, n)])
def test_focus_client_via_load(hlwm, running_clients, running_clients_num, focus):
    layout = '(clients horizontal:{} {})'.format(
        focus, ' '.join(running_clients))

    hlwm.call(['load', layout])

    assert hlwm.call('dump').stdout == layout
    assert hlwm.get_attr('clients.focus.winid') == running_clients[focus]


@pytest.mark.parametrize(
    "running_clients_num,num_bring",
    [(n, f) for n in [1, 3] for f in range(0, n + 1)])
def test_load_brings_windows(hlwm, running_clients, running_clients_num, num_bring):
    hlwm.call('add other')
    layout = '(clients horizontal:0{}{})'.format(
        (' ' if num_bring > 0 else ''),
        ' '.join(running_clients[0:num_bring]))
    assert int(hlwm.get_attr('tags.0.client_count')) \
        == len(running_clients)
    assert int(hlwm.get_attr('tags.1.client_count')) == 0

    hlwm.call(['load', 'other', layout])

    assert int(hlwm.get_attr('tags.0.client_count')) == \
        len(running_clients) - num_bring
    assert int(hlwm.get_attr('tags.1.client_count')) == num_bring
    assert hlwm.call('dump other').stdout == layout


def test_load_invalid_tag(hlwm):
    hlwm.call_xfail(['load', 'invalidtagname', '(clients vertical:0)']) \
        .expect_stderr(r'Tag.*not found')


def test_fraction_precision(hlwm):
    values = [
        '0.4', '0.305', '0.8987',
        '0.5', '0.4001'
    ]
    layout_format = '(split horizontal:{}:0 (clients max:0) (clients max:0))'
    for v in values:
        layout = layout_format.format(v)
        hlwm.call(['load', layout])
        assert hlwm.call('dump').stdout == layout


def test_fraction_precision_outside_range(hlwm):
    # here, we test the decimal i/o for values that are outside
    # of the allowed frame-split-ratio. This test only makes sense
    # because we know that in FrameParser::buildTree(), the already
    # parsed decimal is used for the error message
    values = [
        '0.098',
        '-0.098',
        '-0.5',
        '12.43',
        '-110.01',
    ]
    layout_format = '(split horizontal:{}:0 (clients max:0) (clients max:0))'
    for v in values:
        layout = layout_format.format(v)
        hlwm.call_xfail(['load', layout]) \
            .expect_stderr('but actually is ' + v)


def test_load_floating_client(hlwm):
    winid, _ = hlwm.create_client()
    hlwm.call(f'set_attr clients.{winid}.floating true')
    hlwm.call('set_layout max')
    assert hlwm.call('dump').stdout.rstrip() == '(clients max:0)'

    # soak the client into the frame tree
    layout = f'(clients max:0 {winid})'
    hlwm.call(['load', layout])

    assert hlwm.call('dump').stdout.rstrip() == layout
    assert hlwm.get_attr(f'clients.{winid}.floating') == 'false'


@pytest.mark.parametrize("othertag,minimized", [
    # all combinations where at least one of the flags is True
    # such that it is not in the tiling layer of the first tag yet
    # and such that it is invisible initially
    (True, True), (True, False), (False, True)
])
@pytest.mark.parametrize("floating", [True, False])
def test_load_minimized_client(hlwm, othertag, minimized, floating):
    if othertag:
        hlwm.call('add othertag')
        hlwm.call('rule tag=othertag')
    winid, _ = hlwm.create_client()
    if minimized:
        hlwm.call(f'set_attr clients.{winid}.minimized {hlwm.bool(minimized)}')
    hlwm.call(f'set_attr clients.{winid}.floating {hlwm.bool(floating)}')
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'false'

    # ensure the client is not yet in the tiling layer
    hlwm.call('set_layout max')
    assert hlwm.call('dump').stdout.rstrip() == '(clients max:0)'

    layout = f'(clients max:0 {winid})'
    hlwm.call(['load', layout])
    assert hlwm.call('dump').stdout.rstrip() == layout
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'true'
    assert hlwm.get_attr(f'clients.{winid}.minimized') == 'false'
    assert hlwm.get_attr(f'clients.{winid}.floating') == 'false'
