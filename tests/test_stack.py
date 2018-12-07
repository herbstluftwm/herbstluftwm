import re
import pytest

def helper_get_stack_as_list(hlwm):
    # extract all window IDs out of the stack command
    return re.findall(r'0x[0-9a-f]+', hlwm.call('stack').stdout)

# in tiling mode, the focus layer is used. (similar to floating mode with
# raise_on_focus_temporarily = true). So we test the stacking in floating
# mode.

@pytest.mark.parametrize('count', [2, 5])
def test_clients_stacked_in_reverse_order_of_creation(hlwm, count):
    hlwm.call('floating', 'on')

    clients = hlwm.create_clients(count)

    clients.reverse()
    assert helper_get_stack_as_list(hlwm)[:-1] == clients


def test_raise_client_already_on_top(hlwm):
    hlwm.call('floating', 'on')
    c1, c2 = hlwm.create_clients(2)

    hlwm.call('raise', c2)

    assert helper_get_stack_as_list(hlwm)[:-1] == [c2, c1]


def test_raise_bottom_client(hlwm):
    hlwm.call('floating', 'on')
    c1, c2 = hlwm.create_clients(2)

    hlwm.call('raise', c1)

    assert helper_get_stack_as_list(hlwm)[:-1] == [c1, c2]
