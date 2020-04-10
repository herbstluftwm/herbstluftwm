import pytest


@pytest.mark.parametrize('clientFocused', [0, 1, 2, 3])
def test_directional_focus(hlwm, clientFocused):
    hlwm.call('attr tags.focus.floating on')
    monrect = hlwm.call('monitor_rect').stdout.split(' ')
    width = int(monrect[2])
    height = int(monrect[3])
    client_pos = [
        (0, 0),                     # position of 0
        (width / 2, 0),             # position of 1
        (0, height / 2),            # position of 2
        (width / 2, height / 2),    # position of 3
    ]
    clients = []
    for pos in client_pos:
        winid, _ = hlwm.create_client(position=pos)
        clients.append(winid)
    # clients:
    #    +---+---+
    #    | 0 | 1 |
    #    +---+---+
    #    | 2 | 3 |
    #    +---+---+
    focusDirs = ['up', 'down', 'left', 'right']
    client2neighbours = [
        [None, 2, None, 1],  # neighbours of 0
        [None, 3, 0, None],  # neighbours of 1
        [0, None, None, 3],  # neighbours of 2
        [1, None, 2, None],  # neighbours of 3
    ]

    for idx, d in enumerate(focusDirs):
        # go to 'clientFocused'
        hlwm.call(['jumpto', clients[clientFocused]])

        cmd = ['focus', d]
        expected_target = client2neighbours[clientFocused][idx]
        if expected_target is not None:
            hlwm.call(cmd)
            assert hlwm.get_attr('clients.focus.winid') \
                == clients[expected_target]
        else:
            hlwm.call_xfail(cmd).expect_stderr('No neighbour found')


def test_floating_command_no_tag(hlwm):
    assert hlwm.get_attr('tags.0.floating') == hlwm.bool(False)

    # toggles the floating state of current tag
    hlwm.call('floating')
    assert hlwm.get_attr('tags.0.floating') == hlwm.bool(True)

    # toggles the floating state again
    hlwm.call('floating')
    assert hlwm.get_attr('tags.0.floating') == hlwm.bool(False)
