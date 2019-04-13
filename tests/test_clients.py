import pytest
import time


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
    command = []
    if urgent:
        # prepent urgent command
        command.append(r"echo -e '\a'")
    command += [
        "sleep infinity",
    ]
    # first create a dummy window, such that the second
    # window is not focused and thus keeps the urgent flag
    winid_focus, _ = hlwm.create_client()
    command = '; '.join(command)
    winid, _ = hlwm.create_client(term_command=command)
    # This is racy, however the 'echo' should be evaluated
    # even before the terminal shows up
    time.sleep(0.2)
    assert x11.is_window_urgent(winid) == urgent
    assert hlwm.get_attr('clients.{}.urgent'.format(winid)) \
        == hlwm.bool(urgent)


@pytest.mark.filterwarnings("ignore:tostring")
def test_urgent_after_start(hlwm, x11):
    winid_focus, _ = hlwm.create_client()
    winid, _ = hlwm.create_client()
    assert hlwm.get_attr('clients.{}.urgent'.format(winid)) == 'false'
    assert x11.is_window_urgent(winid) == False

    # make the client urgent:
    x11.make_window_urgent(winid)

    assert hlwm.get_attr('clients.{}.urgent'.format(winid)) == 'true'
    assert x11.is_window_urgent(winid) == True


@pytest.mark.parametrize("explicit_winid", [True, False])
def test_urgent_jumpto(hlwm, explicit_winid):
    winid_old_focus, _ = hlwm.create_client()
    command = r"echo -e '\a' ; sleep infinity"
    winid, _ = hlwm.create_client(term_command=command)
    assert hlwm.get_attr('clients.focus.winid') != winid

    hlwm.call(['jumpto', winid if explicit_winid else 'urgent'])

    assert hlwm.get_attr('clients.focus.winid') == winid
    assert hlwm.get_attr('clients.focus.urgent') == 'false'
