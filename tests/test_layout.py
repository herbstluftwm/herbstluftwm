import pytest

@pytest.mark.parametrize("running_clients_num", [0, 1, 2])
def test_single_frame_layout(hlwm, running_clients, running_clients_num):
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    assert int(hlwm.get_attr('tags.0.client_count')) == running_clients_num
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == running_clients_num


def test_single_frame_layout_three(hlwm):
    hlwm.create_clients(3)
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    assert int(hlwm.get_attr('tags.0.client_count')) == 3
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == 3


@pytest.mark.parametrize("running_clients_num", [2, 3])
def test_explode(hlwm, running_clients, running_clients_num):
    assert running_clients_num >= 2, "explode behaves as auto for one client"
    hlwm.call('split explode')
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    number_upper = (running_clients_num + 1) // 2
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == number_upper
    assert int(hlwm.get_attr('tags.0.client_count')) == running_clients_num
    assert hlwm.get_attr('tags.0.frame_count') == '2'


@pytest.mark.parametrize("running_clients_num", [0, 1, 4])
def test_remove(hlwm, running_clients, running_clients_num):
    hlwm.call('split explode')
    hlwm.call('remove')
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == running_clients_num
    assert int(hlwm.get_attr('tags.0.client_count')) == running_clients_num
    assert hlwm.get_attr('tags.0.frame_count') == '1'
    # TODO: reasonably handle focus, e.g. to have
    #assert hlwm.get_attr('tags.0.curframe_windex') == '2'


@pytest.mark.parametrize("running_clients_num", [3, 4])
def test_focus_wrap(hlwm, running_clients, running_clients_num):
    assert running_clients_num >= 2, "explode behaves as auto for one client"
    # TODO: create the layout explicitly once we have the 'load' command again
    #       instead of calling explode
    hlwm.call('split explode')
    for i in range(0, running_clients_num):
        expected_idx = i % ((running_clients_num + 1) // 2)
        assert int(hlwm.get_attr('tags.0.curframe_windex')) == expected_idx
        if i < running_clients_num - 1:
            hlwm.call('focus down')


@pytest.mark.parametrize("path", '@ 0 1 00 11 . / /. ./'.split(' '))
@pytest.mark.parametrize("running_clients_num", [3])
@pytest.mark.parametrize("num_splits", [0, 1, 2])
def test_dump(hlwm, running_clients, path, running_clients_num, num_splits):
    for i in range(0, num_splits):
        hlwm.call('split explode')
    layout = hlwm.call('dump').stdout
    layout_part = hlwm.call('dump "" ' + path).stdout
    if num_splits > 0:
        assert layout_part in layout
        assert len(layout_part) < len(layout)
    else:
        assert layout_part == layout

