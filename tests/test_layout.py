
def explode_and_remove(hlwm,create_client,number_clients):
    assert number_clients >= 2, "explode behaves as auto for one client"
    for i in range(0, number_clients):
        create_client()
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == number_clients
    assert hlwm.get_attr('tags.0.frame_count') == '1'
    hlwm.callstr('split explode')
    # half of the clients in the upper frame, the other in the lower
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    number_upper = (number_clients + 1) // 2
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == number_upper
    assert int(hlwm.get_attr('tags.0.client_count')) == number_clients
    assert hlwm.get_attr('tags.0.frame_count') == '2'
    # after calling 'focus down' for number_upper-times, the focus switches to
    # the lower frame
    for i in range(0, number_upper):
        assert int(hlwm.get_attr('tags.0.curframe_windex')) == i
        assert int(hlwm.get_attr('tags.0.curframe_wcount')) == number_upper
        hlwm.callstr('focus down')
    # after the last 'focus down' we fall into the other frame
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) \
            == number_clients - number_upper
    assert hlwm.get_attr('tags.0.frame_count') == '2'

    hlwm.call('remove')
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == number_clients
    assert int(hlwm.get_attr('tags.0.client_count')) == number_clients
    assert hlwm.get_attr('tags.0.frame_count') == '1'
    # TODO: reasonably handle focus, e.g. to have
    #assert hlwm.get_attr('tags.0.curframe_windex') == '2'

def test_explode_remove_2(hlwm,create_client):
    explode_and_remove(hlwm,create_client, 2)

def test_explode_remove_3(hlwm,create_client):
    explode_and_remove(hlwm,create_client, 3)

def test_explode_remove_4(hlwm,create_client):
    explode_and_remove(hlwm,create_client, 4)

