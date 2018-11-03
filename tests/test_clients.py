
def test_first_client_gets_focus(hlwm):
    hlwm.fails('get_attr', 'clients.focus.winid')
    (proc,winid) = hlwm.create_client()
    assert hlwm.get_attr('clients.focus.winid') == winid
    # let the client die once the x display is closed
    # TODO: close the client. this currently lets hlwm crash
    #proc.terminate()
    #proc.wait(2)
