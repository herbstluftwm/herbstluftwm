import re

def helper_get_stack_as_list(hlwm):
    # extract all window IDs out of the stack command
    return re.findall(r'0x[0-9a-f]+', hlwm.call('stack').stdout)

# in tiling mode, the focus layer is used. (similar to floating mode with
# raise_on_focus_temporarily = true). So we test the stacking in floating
# mode.

def test_latest_client_on_top(hlwm, create_client):
    hlwm.call('floating', 'on')
    # new clients always appear on top of the stacking order
    # really create clients in the right oder
    c1 = create_client()
    c2 = create_client()
    assert helper_get_stack_as_list(hlwm)[0:2] == [c2,c1]

def test_raise_first_client(hlwm, create_clients):
    hlwm.call('floating', 'on')
    [c1,c2] = create_clients(2)
    hlwm.call('raise', c2)
    assert helper_get_stack_as_list(hlwm)[0:2] == [c2,c1]

def test_raise_second_client(hlwm, create_clients):
    hlwm.call('floating', 'on')
    [c1,c2] = create_clients(2)
    hlwm.call('raise', c1)
    assert helper_get_stack_as_list(hlwm)[0:2] == [c1,c2]
