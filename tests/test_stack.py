import re
import pytest


def strip_winids(string):
    """
    Replaces all substrings that look like window IDs with a fixed string.
    """
    return re.sub(r'0x([0-9a-f]+)', '<windowid>', string)


def helper_get_stack_as_list(hlwm, clients_only=True, strip_focus_layer=False):
    stack_stdout = hlwm.call('stack').stdout
    if strip_focus_layer:
        # remove all lines after the "Focus-Layer"-line
        # that contain the word "Client". Note that . does not
        # match newline.
        stack_stdout = re.sub('Focus-Layer(\n.*Client.*)*',
                              'Focus-Layer',
                              stack_stdout,
                              flags=re.MULTILINE)
    if clients_only:
        matches = re.finditer('Client (0x[0-9a-f]+)', stack_stdout)
        winids = [m.group(1) for m in matches]
    else:
        # extract all window IDs out of the stack command
        winids = re.findall(r'0x[0-9a-f]+', stack_stdout)
    # remove duplicates
    winids_new = []
    for w in winids:
        if w not in winids_new:
            winids_new.append(w)
    return winids_new


@pytest.mark.parametrize('floatingmode', ['on', 'off'])
@pytest.mark.parametrize('count', [2, 5])
def test_clients_stacked_in_reverse_order_of_creation(hlwm, floatingmode, count):
    hlwm.call(['floating', floatingmode])

    clients = hlwm.create_clients(count)

    clients.reverse()
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == clients


@pytest.mark.parametrize('floatingmode', ['on', 'off'])
def test_raise_client_already_on_top(hlwm, floatingmode):
    hlwm.call(['floating', floatingmode])
    c1, c2 = hlwm.create_clients(2)

    hlwm.call(['raise', c2])

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == [c2, c1]


def test_raise_bottom_client(hlwm):
    hlwm.call('floating on')
    c1, c2 = hlwm.create_clients(2)

    hlwm.call(['raise', c1])

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == [c1, c2]


def create_two_monitors_with_client_each(hlwm):
    hlwm.call('add tag2')
    hlwm.call('set_attr tags.0.floating on')
    hlwm.call('set_attr tags.1.floating on')
    hlwm.call('add_monitor 800x600+40+40 tag2')
    c1, c2 = hlwm.create_clients(2)
    hlwm.call(['load', 'tag2', '(clients max:0 {})'.format(c2)])
    return [c1, c2]


def test_new_monitor_is_on_top(hlwm):
    [c1, c2] = create_two_monitors_with_client_each(hlwm)

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == [c2, c1]


def test_raise_monitor_already_on_top(hlwm):
    [c1, c2] = create_two_monitors_with_client_each(hlwm)

    hlwm.call('raise_monitor 1')

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == [c2, c1]


def test_raise_monitor_2(hlwm):
    [c1, c2] = create_two_monitors_with_client_each(hlwm)

    hlwm.call('raise_monitor 0')

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == [c1, c2]


@pytest.mark.parametrize("here_float", [True, False])
@pytest.mark.parametrize("there_float", [True, False])
@pytest.mark.parametrize("client_float", [True, False])
@pytest.mark.parametrize("method", ['move', 'bring'])
def test_moving_with_floating(hlwm, here_float, there_float, client_float, method):
    hlwm.call('add there')
    winid, _ = hlwm.create_client()
    hlwm.call(['set_attr', 'tags.focus.floating', hlwm.bool(here_float)])
    hlwm.call(['set_attr', 'tags.by-name.there.floating', hlwm.bool(there_float)])
    hlwm.call(['set_attr', 'clients.focus.floating', hlwm.bool(client_float)])

    if method == 'move':
        hlwm.call('move there')
    else:  # method == 'bring':
        hlwm.call('use there')
        hlwm.call(['bring', winid])
        hlwm.call('use_previous')

    assert hlwm.get_attr('clients.{}.tag'.format(winid)) == 'there'
    assert winid not in helper_get_stack_as_list(hlwm)
    hlwm.call('use there')
    assert hlwm.get_attr('clients.focus.winid') == winid
    assert winid in helper_get_stack_as_list(hlwm)


def test_stack_tree(hlwm):
    # Simplified tree style:
    hlwm.call('set tree_style "     - -"')

    # Populate the stack:
    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+40+40 tag2 monitor2')
    hlwm.create_client()
    hlwm.call('focus_monitor monitor2')
    hlwm.call('split left')
    hlwm.create_client()
    # TODO: Make one client fullscreen (doesn't seem to work yet)

    stack = hlwm.call('stack')

    expected_stack = '''\
  -
    - Monitor 1 ("monitor2") with tag "tag2"
      - Focus-Layer
      - Fullscreen-Layer
      - Floating-Layer
      - Tiling-Layer
        - Client <windowid> "bash"
      - Frame Layer
        - Window <windowid>
        - Window <windowid>
    - Monitor 0 with tag "default"
      - Focus-Layer
      - Fullscreen-Layer
      - Floating-Layer
      - Tiling-Layer
        - Client <windowid> "bash"
      - Frame Layer
        - Window <windowid>
'''
    assert strip_winids(stack.stdout) == expected_stack
