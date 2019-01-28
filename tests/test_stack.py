import re
import pytest

def strip_winids(string):
    """
    Replaces all substrings that look like window IDs with a fixed string.
    """
    return re.sub(r'0x([0-9a-f]+)', '0xanywinid', string)


def helper_get_stack_as_list(hlwm):
    # extract all window IDs out of the stack command
    return re.findall(r'0x[0-9a-f]+', hlwm.call('stack').stdout)

# in tiling mode, the focus layer is used. (similar to floating mode with
# raise_on_focus_temporarily = true). So we test the stacking in floating
# mode.

@pytest.mark.parametrize('count', [2, 5])
def test_clients_stacked_in_reverse_order_of_creation(hlwm, count):
    hlwm.call('floating on')

    clients = hlwm.create_clients(count)

    clients.reverse()
    assert helper_get_stack_as_list(hlwm)[:-1] == clients


def test_raise_client_already_on_top(hlwm):
    hlwm.call('floating on')
    c1, c2 = hlwm.create_clients(2)

    hlwm.call(['raise', c2])

    assert helper_get_stack_as_list(hlwm)[:-1] == [c2, c1]


def test_raise_bottom_client(hlwm):
    hlwm.call('floating on')
    c1, c2 = hlwm.create_clients(2)

    hlwm.call(['raise', c1])

    assert helper_get_stack_as_list(hlwm)[:-1] == [c1, c2]


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
        - Client 0x800022 "sleep infinity"
      - Fullscreen-Layer
      - Normal Layer
        - Client 0x800022 "sleep infinity"
      - Frame Layer
        - Window 0x200012
        - Window 0x20000a
    - Monitor 0 with tag "default"
      - Focus-Layer
        - Client 0x600022 "sleep infinity"
      - Fullscreen-Layer
      - Normal Layer
        - Client 0x600022 "sleep infinity"
      - Frame Layer
        - Window 0x200008
'''
    assert strip_winids(stack.stdout) == strip_winids(expected_stack)
