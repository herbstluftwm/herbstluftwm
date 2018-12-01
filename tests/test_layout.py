import pytest

@pytest.mark.parametrize("number_clients", [0,1,2])
def test_single_frame_layout(hlwm,create_client,number_clients):
    for i in range(0, number_clients):
        create_client()
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    assert int(hlwm.get_attr('tags.0.client_count')) == number_clients
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == number_clients

@pytest.mark.parametrize("number_clients", [2,3])
def test_explode(hlwm,create_client,number_clients):
    assert number_clients >= 2, "explode behaves as auto for one client"
    for i in range(0, number_clients):
        create_client()
    hlwm.callstr('split explode')
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    number_upper = (number_clients + 1) // 2
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == number_upper
    assert int(hlwm.get_attr('tags.0.client_count')) == number_clients
    assert hlwm.get_attr('tags.0.frame_count') == '2'

@pytest.mark.parametrize("number_clients", [0,1,4])
def test_remove(hlwm,create_client,number_clients):
    for i in range(0, number_clients):
        create_client()
    hlwm.callstr('split explode')
    hlwm.callstr('remove')
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == number_clients
    assert int(hlwm.get_attr('tags.0.client_count')) == number_clients
    assert hlwm.get_attr('tags.0.frame_count') == '1'
    # TODO: reasonably handle focus, e.g. to have
    #assert hlwm.get_attr('tags.0.curframe_windex') == '2'

@pytest.mark.parametrize("number_clients", [3,4])
def test_focus_wrap(hlwm,create_client,number_clients):
    assert number_clients >= 2, "explode behaves as auto for one client"
    for i in range(0, number_clients):
        create_client()
    # TODO: create the layout explicitly once we have the 'load' command again
    #       instead of calling explode
    hlwm.callstr('split explode')
    for i in range(0, number_clients):
        expected_idx = i % ((number_clients + 1) // 2)
        assert int(hlwm.get_attr('tags.0.curframe_windex')) == expected_idx
        if i < number_clients - 1:
            hlwm.callstr('focus down')

