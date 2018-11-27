def test_first_client_gets_focus(hlwm, create_client):
    hlwm.fails('get_attr', 'clients.focus.winid')
    client = create_client()
    assert hlwm.get_attr('clients.focus.winid') == client


def test_alter_fullscreen(hlwm, create_client):
    create_client()
    positives = ('true', 'on', '1')
    negatives = ('false', 'off', '0')
    for on, off in zip(positives, negatives):
        hlwm.call('attr', 'clients.focus.fullscreen', on)
        assert hlwm.get_attr('clients.focus.fullscreen') == 'true'
        hlwm.call('attr', 'clients.focus.fullscreen', off)
        assert hlwm.get_attr('clients.focus.fullscreen') == 'false'
    # current state is now false
    hlwm.call('attr', 'clients.focus.fullscreen', 'toggle')
    assert hlwm.get_attr('clients.focus.fullscreen') == 'true'
    hlwm.call('attr', 'clients.focus.fullscreen', 'toggle')
    assert hlwm.get_attr('clients.focus.fullscreen') == 'false'
