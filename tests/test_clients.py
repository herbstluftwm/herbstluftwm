import pytest


def test_client_lives_longer_than_hlwm(hlwm):
    # This might seem like a nonsensical test, but it confirms the proper
    # free()ing of memory related to tracked clients. In all other tests,
    # clients are killed before hlwm is terminated. We start more than one
    # client because that appears to be needed so that LeakSanitizer reliably
    # detects the leak (if there is one).
    winid, _ = hlwm.create_client(keep_running=True)
    winid, _ = hlwm.create_client(keep_running=True)


def test_first_client_gets_focus(hlwm):
    hlwm.call_xfail('get_attr clients.focus.winid')
    winid, _ = hlwm.create_client()
    assert hlwm.get_attr('clients.focus.winid') == winid


def test_alter_fullscreen(hlwm):
    hlwm.create_client()
    positives = ('true', 'on', '1')
    negatives = ('false', 'off', '0')
    for on, off in zip(positives, negatives):
        hlwm.call(['attr', 'clients.focus.fullscreen', on])
        assert hlwm.get_attr('clients.focus.fullscreen') == 'true'
        hlwm.call(['attr', 'clients.focus.fullscreen', off])
        assert hlwm.get_attr('clients.focus.fullscreen') == 'false'
    # current state is now false
    hlwm.call('attr clients.focus.fullscreen toggle')
    assert hlwm.get_attr('clients.focus.fullscreen') == 'true'
    hlwm.call('attr clients.focus.fullscreen toggle')
    assert hlwm.get_attr('clients.focus.fullscreen') == 'false'


def test_fullscreen_completion(hlwm):
    assert hlwm.complete("fullscreen") == 'false off on toggle true'.split(' ')


def test_close_without_clients(hlwm):
    assert hlwm.unchecked_call('close').returncode == 3


@pytest.mark.parametrize("running_clients_num", [1, 2, 3, 4])
def test_close(hlwm, running_clients_num):
    hlwm.create_clients(running_clients_num)
    hlwm.call('close')
