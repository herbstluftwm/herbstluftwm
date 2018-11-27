def test_default_tag(hlwm):
    assert hlwm.get_attr('tags.count') == '1'
    assert hlwm.get_attr('tags.0.name') == 'default'


def test_add_tag(hlwm):
    hlwm.callstr('add foobar')

    assert hlwm.get_attr('tags.count') == '2'
    assert hlwm.get_attr('tags.1.name') == 'foobar'


def test_move_focused_client_to_new_tag(hlwm, create_client):
    hlwm.callstr('add foobar')
    create_client()

    hlwm.callstr('move foobar')

    # TODO: Assert that foobar now has 1 client
    # TODO: Assert that winid is now in foobar
