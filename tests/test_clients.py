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


@pytest.mark.filterwarnings("ignore:tostring")
@pytest.mark.parametrize("urgent", [True, False])
def test_urgent_on_start(hlwm, x11, urgent):
    # first create a dummy window, such that the second
    # window is not focused and thus keeps the urgent flag
    hlwm.create_client()  # dummy client that gets the focus
    window, winid = x11.create_client(urgent=urgent)
    assert x11.is_window_urgent(window) == urgent
    assert hlwm.get_attr('clients.{}.urgent'.format(winid)) \
        == hlwm.bool(urgent)


@pytest.mark.filterwarnings("ignore:tostring")
def test_urgent_after_start(hlwm, x11):
    hlwm.create_client()  # dummy client that gets the focus
    winid, _ = hlwm.create_client()
    assert hlwm.get_attr('clients.{}.urgent'.format(winid)) == 'false'
    assert not x11.is_window_urgent(x11.window(winid))

    # make the client urgent:
    x11.make_window_urgent(x11.window(winid))

    assert hlwm.get_attr('clients.{}.urgent'.format(winid)) == 'true'
    assert x11.is_window_urgent(x11.window(winid))


@pytest.mark.parametrize("explicit_winid", [True, False])
def test_urgent_jumpto_resets_urgent_flag(hlwm, x11, explicit_winid):
    hlwm.create_client()  # dummy client that gets the focus
    window, winid = x11.create_client(urgent=True)
    assert hlwm.get_attr('clients.focus.winid') != winid
    assert hlwm.get_attr('clients.{}.urgent'.format(winid)) == hlwm.bool(True)

    hlwm.call(['jumpto', winid if explicit_winid else 'urgent'])

    assert hlwm.get_attr('clients.focus.winid') == winid
    assert hlwm.get_attr('clients.focus.urgent') == 'false'


def test_client_tag_attribute(hlwm):
    othertag = 'othertag'
    winid, _ = hlwm.create_client()
    hlwm.call(['add', othertag])
    clients_win_tag = 'clients.{}.tag'.format(winid)
    assert hlwm.get_attr(clients_win_tag) == hlwm.get_attr('tags.0.name')

    hlwm.call(['move', othertag])

    assert hlwm.get_attr(clients_win_tag) == othertag


def test_client_with_pid(hlwm):
    winid, proc = hlwm.create_client()
    assert int(hlwm.get_attr('clients.focus.pid')) == proc.pid


def test_client_without_pid(hlwm, x11):
    _, winid = x11.create_client()
    # x11.create_client() does not set the _NET_WM_PID property
    assert int(hlwm.get_attr('clients.focus.pid')) == -1
