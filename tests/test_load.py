import pytest
import re


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
    m = re.match(r'^load: Syntax error at ([0-9]*): ', c.stderr)
    assert m is not None
    assert int(m.group(1)) == error_pos


@pytest.mark.parametrize("layout", [
    "(clients max:0)",
    "(clients grid:0)",
    "(clients horizontal:0 0234)",
    "(clients vertical:0 0x2343)",
    "(clients vertical:0 1713)",
    " (  clients   vertical:0  )",
    "(split horizontal:0.3:0)",
    "(split vertical:0.3:0 (clients horizontal:0))",
    "(split vertical:0.3:0 (split vertical:0.4:1))",
])
@pytest.mark.parametrize('num_splits_before', [0, 1, 2])
def test_valid_layout_syntax(hlwm, layout, num_splits_before):
    for i in range(0, num_splits_before):
        hlwm.call('split explode')

    hlwm.call(['load', layout])


@pytest.mark.parametrize("running_clients_num,focus",
    [(n, f) for n in [1, 3] for f in range(0, n)])
def test_focus_client_via_load(hlwm, running_clients, running_clients_num, focus):
    layout = '(clients horizontal:{} {})'.format(
        focus, ' '.join(running_clients))

    hlwm.call(['load', layout])

    assert hlwm.call('dump').stdout == layout
    assert hlwm.get_attr('clients.focus.winid') == running_clients[focus]


@pytest.mark.parametrize("running_clients_num,num_bring",
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



