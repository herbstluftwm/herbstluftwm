import pytest
import random
from herbstluftwm.types import Rectangle


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


@pytest.mark.parametrize("command", ["fullscreen", "pseudotile"])
def test_fullscreen_pseudotile_invalid_arg(hlwm, command):
    hlwm.create_client()
    hlwm.call_xfail([command, 'novalue']) \
        .expect_stderr('illegal argument "novalue"')


def test_fullscreen_completion(hlwm):
    assert hlwm.complete("fullscreen") == 'false off on toggle true'.split(' ')


def test_close_without_clients(hlwm):
    assert hlwm.unchecked_call('close').returncode == 3


@pytest.mark.parametrize("running_clients_num", [0, 1, 4])  # number of unfocused clients
def test_close_focused_client(hlwm, running_clients, running_clients_num):
    hlwm.call('rule focus=on')
    winid, proc = hlwm.create_client()
    assert hlwm.attr.clients.focus.winid() == winid

    hlwm.call('close')

    proc.wait(20)  # wait for the client to shut down
    hlwm.call('true')  # sync with hlwm
    clients = hlwm.list_children('clients')
    # all other clients still run:
    assert winid not in clients
    for other in running_clients:
        assert other in clients


def test_close_unfocused_client(hlwm):
    focused, _ = hlwm.create_client()
    unfocused, proc = hlwm.create_client()
    hlwm.call(['jumpto', focused])
    assert hlwm.attr.clients.focus.winid() == focused

    hlwm.call(['close', unfocused])

    proc.wait(10)  # wait
    # the other client is still running:
    assert hlwm.attr.clients.focus.winid() == focused


def test_close_unmanaged_client(hlwm):
    # even though the client is not managed,
    # the synchronization works because the hooks are
    # still fired.
    hlwm.call('rule once manage=off')
    winid, proc = hlwm.create_client()

    hlwm.call(['close', winid])

    proc.wait(10)  # wait for the client to shut down


def test_close_completion(hlwm):
    winid, _ = hlwm.create_client()
    assert winid in hlwm.complete(['close'])
    hlwm.command_has_all_args(['close', winid])


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


@pytest.mark.parametrize("arg,expected_idx", [
    ('longest-minimized', 0),
    ('last-minimized', -1),
])
@pytest.mark.parametrize("noop", [True, False])
def test_jumpto_longest_minimized_last_minimized(hlwm, noop, arg, expected_idx):
    clients = hlwm.create_clients(5)
    another_client, _ = hlwm.create_client()

    random.shuffle(clients)  # use a different order than creation order

    if noop:
        hlwm.attr.clients[clients[2]].minimized = False  # should not change anything

    for c in clients:
        hlwm.attr.clients[c].minimized = True

    if noop:
        # should not change anything if we re-write the attribute
        # of some other client:
        hlwm.attr.clients[clients[2 + expected_idx]].minimized = True
        hlwm.attr.clients[another_client].minimized = False

    # Add some more clients that should not interfere with the minimized aliases:
    # 1. add another floating client that's not minimized
    other_float, _ = hlwm.create_client()
    hlwm.attr.clients[other_float].floating = True
    hlwm.attr.clients[other_float].minimized = False
    # 2. minimize another client but put it on another tag:
    hlwm.call('add othertag')
    hlwm.call('rule once tag=othertag')
    on_other_tag, _ = hlwm.create_client()
    hlwm.attr.clients[on_other_tag].minimized = True

    hlwm.call(['jumpto', arg])

    assert hlwm.attr.clients.focus.winid() == clients[expected_idx]
    assert hlwm.attr.clients.focus.minimized() is False


def test_minimized_client_completions(hlwm):
    assert 'last-minimized' in hlwm.complete('jumpto')
    assert 'longest-minimized' in hlwm.complete('jumpto')


def test_minimized_client_invalid_arg(hlwm):
    hlwm.call_xfail('jumpto longest-minimized') \
        .expect_stderr('No client is minimized')

    hlwm.call_xfail('jumpto last-minimized') \
        .expect_stderr('No client is minimized')


@pytest.mark.parametrize("ewmhstate", [True, False])
@pytest.mark.parametrize("hlwmstate", [True, False])
def test_fullscreen_ewmhnotify(hlwm, x11, ewmhstate, hlwmstate):
    window, winid = x11.create_client()

    def set_attr_bool(key, value):
        attribute = 'clients.{}.{}'.format(winid, key)
        hlwm.call(['set_attr', attribute, hlwm.bool(value)])
    # set the ewmh fullscreen state
    set_attr_bool('ewmhnotify', True)
    set_attr_bool('fullscreen', ewmhstate)

    # set the hlwm/actual fullscreen state
    set_attr_bool('ewmhnotify', False)
    set_attr_bool('fullscreen', hlwmstate)

    expected = ('_NET_WM_STATE_FULLSCREEN' in x11.ewmh.getWmState(window, str=True))
    assert expected == ewmhstate


def test_client_tag_attribute(hlwm):
    winid, _ = hlwm.create_client()
    hlwm.call('add othertag')
    clients_win_tag = 'clients.{}.tag'.format(winid)
    assert hlwm.get_attr(clients_win_tag) == hlwm.get_attr('tags.0.name')

    hlwm.call('move othertag')

    assert hlwm.get_attr(clients_win_tag) == 'othertag'


def test_client_with_pid(hlwm, x11):
    _, winid = x11.create_client(pid=2342)
    assert int(hlwm.get_attr('clients.focus.pid')) == 2342


def test_client_without_pid(hlwm, x11):
    _, winid = x11.create_client(pid=None)
    assert int(hlwm.get_attr('clients.focus.pid')) == -1


def test_client_wm_class(hlwm, x11):
    _, winid = x11.create_client(wm_class=('myinst', 'myclass'))
    assert hlwm.get_attr('clients.{}.instance'.format(winid)) == 'myinst'
    assert hlwm.get_attr('clients.{}.class'.format(winid)) == 'myclass'


def test_client_wm_class_none(hlwm, x11):
    _, winid = x11.create_client(wm_class=None)
    assert hlwm.get_attr('clients.{}.instance'.format(winid)) == ''
    assert hlwm.get_attr('clients.{}.class'.format(winid)) == ''


def test_bring_from_different_tag(hlwm, x11):
    _, bonnie = x11.create_client()
    hlwm.call('true')
    hlwm.call('add anothertag')
    hlwm.call('use anothertag')
    assert hlwm.get_attr(f'clients.{bonnie}.tag') == 'default'

    hlwm.call(['bring', bonnie])

    assert hlwm.get_attr(f'clients.{bonnie}.tag') == 'anothertag'
    assert hlwm.get_attr('clients.focus.winid') == bonnie


def test_bring_from_same_tag_different_frame(hlwm, x11):
    hlwm.call('split horizontal')
    hlwm.call('focus right')
    _, winid = x11.create_client()
    hlwm.call('true')
    hlwm.call('focus left')
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == 0

    hlwm.call(['bring', winid])
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == 1
    assert hlwm.get_attr('clients.focus.winid') == winid


@pytest.mark.parametrize("command", ['fullscreen', 'pseudotile'])
def test_fullscreen_pseudotile_command(hlwm, command):
    # create three clients to check that the command
    # adjusts the property of the correct client
    hlwm.create_clients(3)
    for set_value in ['true', 'false', 'toggle', '']:
        old_value = hlwm.get_attr(f'clients.focus.{command}')
        # let shlex parse it such that for '', no argument is passed
        hlwm.call(f'{command} {set_value}')
        new_value = hlwm.get_attr(f'clients.focus.{command}')
        if set_value in ['', 'toggle']:
            assert old_value != new_value
        else:
            assert new_value == set_value


@pytest.mark.parametrize("attribute", [
    'ewmhnotify',
    'ewmhrequests',
    'fullscreen',
    'pseudotile',
    'sizehints_floating',
    'sizehints_tiling',
])
def test_bool_attributes_writable(hlwm, attribute):
    hlwm.create_clients(1)
    for value in ['true', 'false', 'toggle']:
        hlwm.call(f'set_attr clients.focus.{attribute} {value}')


@pytest.mark.parametrize("floating", ['on', 'off'])
def test_minimization_of_visible_client(hlwm, floating):
    winid, _ = hlwm.create_client()
    hlwm.call(f'set_attr clients.{winid}.floating {floating}')
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'true'

    hlwm.call(f'set_attr clients.{winid}.minimized on')
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'false'

    hlwm.call(f'set_attr clients.{winid}.minimized off')
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'true'


@pytest.mark.parametrize("floating", ['on', 'off'])
def test_minimization_on_other_tag(hlwm, floating):
    hlwm.call('add othertag')
    hlwm.call('rule tag=othertag')
    winid, _ = hlwm.create_client()

    hlwm.call(f'set_attr clients.{winid}.floating {floating}')
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'false'

    hlwm.call(f'set_attr clients.{winid}.minimized on')
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'false'

    hlwm.call(f'set_attr clients.{winid}.minimized off')
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'false'


@pytest.mark.parametrize("floating", ['on', 'off'])
def test_minimization_focus_other_tag(hlwm, floating):
    # place a minimized client on an invisible tag
    # so the client should be invisible, too
    hlwm.call('add othertag')
    hlwm.call('rule tag=othertag focus=on')
    winid, _ = hlwm.create_client()
    hlwm.create_client()  # another client taking the focus
    hlwm.call(f'set_attr clients.{winid}.floating {floating}')
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'false'
    hlwm.call(f'set_attr clients.{winid}.minimized on')
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'false'

    # focus the other tag
    hlwm.call('use othertag')

    # now the client stays invisible since it's minimized
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'false'
    # client becomes visible on un-minimization
    hlwm.call(f'set_attr clients.{winid}.minimized off')
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'true'


@pytest.mark.parametrize("floating", ['on', 'off'])
def test_minimization_focus_window(hlwm, floating):
    winid, _ = hlwm.create_client()
    hlwm.call(f'set_attr clients.{winid}.minimized on')
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'false'
    assert 'focus' not in hlwm.list_children('clients')

    # focus the client
    hlwm.call(f'jumpto {winid}')

    assert hlwm.get_attr(f'clients.{winid}.visible') == 'true'
    assert hlwm.get_attr('clients.focus.winid') == winid


def test_minimize_only_floating_client(hlwm):
    hlwm.call('rule floating=on focus=on')
    winid, _ = hlwm.create_client()
    assert hlwm.get_attr('clients.focus.winid') == winid
    assert hlwm.get_attr('tags.focus.floating_focused') == 'true'

    hlwm.call(f'set_attr clients.{winid}.minimized on')

    assert hlwm.get_attr('tags.focus.floating_focused') == 'false'


def test_parent_frame_attribute_one_split(hlwm):
    c1, c2 = hlwm.create_clients(2)
    hlwm.call('split explode')

    assert hlwm.attr.clients[c1].parent_frame.index() == '0'
    assert hlwm.attr.clients[c2].parent_frame.index() == '1'


def test_parent_frame_attribute_no_split(hlwm):
    c1, c2 = hlwm.create_clients(2)

    assert hlwm.attr.clients[c1].parent_frame.index() == ''
    assert hlwm.attr.clients[c2].parent_frame.index() == ''


def test_parent_frame_attribute_window_floating(hlwm):
    winid, _ = hlwm.create_client()
    # do it in a loop to verify that we don't have typos in the assert
    for value in ['on', 'off']:
        hlwm.attr.clients[winid].floating = value

        assert ('parent_frame' in hlwm.list_children(f'clients.{winid}')) \
            == (value == 'off')


def test_parent_frame_attribute_tag_floating(hlwm):
    winid, _ = hlwm.create_client()
    # do it in a loop to verify that we don't have typos in the assert
    for value in ['on', 'off']:
        hlwm.attr.tags.focus.floating = value

        assert ('parent_frame' in hlwm.list_children(f'clients.{winid}')) \
            == (value == 'off')


@pytest.mark.parametrize("minimized", [True, False])
@pytest.mark.parametrize("floating", [True, False])
@pytest.mark.parametrize("othertag", [True, False])
def test_floating_geometry_change(hlwm, minimized, floating, x11, othertag):
    if othertag:
        hlwm.call('add othertag')
        hlwm.call('rule tag=othertag')
    handle, winid = x11.create_client()
    clientobj = hlwm.attr.clients[winid]
    clientobj.floating = hlwm.bool(floating)
    clientobj.minimized = hlwm.bool(minimized)
    hlwm.call('pad "" 0 0 0 0')
    hlwm.call('move_monitor "" 800x600+0+0')

    for geom in [Rectangle(x=50, y=100, width=100, height=300),
                 Rectangle(x=-20, y=2, width=430, height=200)]:

        clientobj.floating_geometry = geom.to_user_str()

        if not minimized and not othertag:
            assert (clientobj.content_geometry() == geom) == floating
            assert (x11.get_absolute_top_left(handle) == (geom.x, geom.y)) == floating
