import pytest


def test_default_tag_exists_and_has_name(hlwm):
    assert hlwm.get_attr('tags.count') == '1'
    assert hlwm.get_attr('tags.0.name') == 'default'


def test_add_tag(hlwm):
    hlwm.call('add foobar')

    assert hlwm.get_attr('tags.count') == '2'
    assert hlwm.get_attr('tags.1.client_count') == '0'
    assert hlwm.get_attr('tags.1.client_count') == '0'
    assert hlwm.get_attr('tags.1.curframe_wcount') == '0'
    assert hlwm.get_attr('tags.1.curframe_windex') == '0'
    assert hlwm.get_attr('tags.1.frame_count') == '1'
    assert hlwm.get_attr('tags.1.index') == '1'
    assert hlwm.get_attr('tags.1.name') == 'foobar'


@pytest.mark.parametrize("running_clients_num", [0, 1, 5])
def test_new_clients_increase_client_count(hlwm, running_clients, running_clients_num):
    assert hlwm.get_attr('tags.0.client_count') == str(running_clients_num)


def test_move_focused_client_to_new_tag(hlwm):
    hlwm.call('add foobar')
    assert hlwm.get_attr('tags.0.client_count') == '0'
    assert hlwm.get_attr('tags.1.client_count') == '0'

    hlwm.create_client()
    assert hlwm.get_attr('tags.0.client_count') == '1'
    assert hlwm.get_attr('tags.1.client_count') == '0'

    hlwm.call('move foobar')

    assert hlwm.get_attr('tags.0.client_count') == '0'
    assert hlwm.get_attr('tags.0.curframe_wcount') == '0'
    assert hlwm.get_attr('tags.1.client_count') == '1'
    assert hlwm.get_attr('tags.1.curframe_wcount') == '1'
    # TODO: Assert that winid is now in foobar


def test_merge_tag_into_another_tag(hlwm):
    hlwm.call('add foobar')
    hlwm.create_client()
    hlwm.call('use_index 1')

    hlwm.call('merge_tag default foobar')

    assert hlwm.get_attr('tags.count') == '1'
    assert hlwm.get_attr('tags.0.name') == 'foobar'
