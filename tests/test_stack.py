import re
import pytest


def strip_winids(string):
    """
    Replaces all substrings that look like window IDs with a fixed string.
    """
    return re.sub(r'0x([0-9a-f]+)', '<windowid>', string)


def helper_get_stack_as_list(hlwm, clients_only=True):
    if clients_only:
        matches = re.finditer('Client (0x[0-9a-f]+)', hlwm.call('stack').stdout)
        return [m.group(1) for m in matches]
    else:
        # extract all window IDs out of the stack command
        return re.findall(r'0x[0-9a-f]+', hlwm.call('stack').stdout)


@pytest.mark.parametrize('floatingmode', ['on', 'off'])
@pytest.mark.parametrize('count', [2, 5])
def test_clients_stacked_in_reverse_order_of_creation(hlwm, floatingmode, count):
    hlwm.call(['floating', floatingmode])

    clients = hlwm.create_clients(count)

    clients.reverse()
    assert helper_get_stack_as_list(hlwm) == clients


@pytest.mark.parametrize('floatingmode', ['on', 'off'])
def test_raise_client_already_on_top(hlwm, floatingmode):
    hlwm.call(['floating', floatingmode])
    c1, c2 = hlwm.create_clients(2)

    hlwm.call(['raise', c2])

    assert helper_get_stack_as_list(hlwm) == [c2, c1]


def test_raise_bottom_client(hlwm):
    hlwm.call('floating on')
    c1, c2 = hlwm.create_clients(2)

    hlwm.call(['raise', c1])

    assert helper_get_stack_as_list(hlwm) == [c1, c2]


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

    assert helper_get_stack_as_list(hlwm) == [c2, c1]


def test_raise_monitor_already_on_top(hlwm):
    [c1, c2] = create_two_monitors_with_client_each(hlwm)

    hlwm.call('raise_monitor 1')

    assert helper_get_stack_as_list(hlwm) == [c2, c1]


def test_raise_monitor_2(hlwm):
    [c1, c2] = create_two_monitors_with_client_each(hlwm)

    hlwm.call('raise_monitor 0')

    assert helper_get_stack_as_list(hlwm) == [c1, c2]


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
      - Normal Layer
        - Client <windowid> "bash"
      - Frame Layer
        - Window <windowid>
        - Window <windowid>
    - Monitor 0 with tag "default"
      - Focus-Layer
      - Fullscreen-Layer
      - Normal Layer
        - Client <windowid> "bash"
      - Frame Layer
        - Window <windowid>
'''
    assert strip_winids(stack.stdout) == expected_stack
