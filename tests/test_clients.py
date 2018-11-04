
def test_first_client_gets_focus(hlwm):
    hlwm.fails('get_attr', 'clients.focus.winid')
    (proc,winid) = hlwm.create_client()
    assert hlwm.get_attr('clients.focus.winid') == winid
    # let the client die once the x display is closed
    # TODO: close the client. this currently lets hlwm crash
    proc.terminate()
    proc.wait(2)

def test_alter_fullscreen(hlwm):
    (proc,winid) = hlwm.create_client()
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
    proc.terminate()
    proc.wait(2)

