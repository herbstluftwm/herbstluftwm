import pytest
import re
import math

# Note: For unknown reasons, mouse buttons 4 and 5 (scroll wheel) do not work
# in Xvfb when running tests on Travis. Therefore, we maintain two lists of
# buttons:
MOUSE_BUTTONS_THAT_EXIST = [1, 2, 3, 4, 5]
MOUSE_BUTTONS_THAT_WORK = [1, 2, 3]


@pytest.mark.parametrize('method', ['-F', '--all'])
def test_mouseunbind_all(hlwm, method, mouse):
    hlwm.create_client()
    hlwm.call('mousebind Button1 call quit')

    unbind = hlwm.call(['mouseunbind', method])

    assert unbind.stdout == ''
    # TODO: assert hlwm.call('list_mousebind').stdout == ''
    mouse.click('1')  # verify that binding got ungrabbed


def test_mousebind_empty_command(hlwm):
    call = hlwm.call_xfail('mousebind Button3 call')
    call.expect_stderr('mousebind: not enough arguments')


def test_mousebind_unknown_button(hlwm):
    call = hlwm.call_xfail('mousebind Button42 call quit')
    call.expect_stderr('mousebind: Unknown mouse button "Button42"')


def test_mousebind_unknown_action(hlwm):
    call = hlwm.call_xfail('mousebind Button1 get schwifty')
    call.expect_stderr('mousebind: Unknown mouse action "get"')


@pytest.mark.parametrize('button', MOUSE_BUTTONS_THAT_WORK)
def test_trigger_mouse_binding_without_modifier(hlwm, mouse, button):
    hlwm.call('new_attr string my_press')
    hlwm.call(f'mousebind Button{button} call set_attr my_press yup')
    client_id, _ = hlwm.create_client()

    mouse.click(str(button), client_id)

    assert hlwm.get_attr('my_press') == 'yup'


@pytest.mark.parametrize('button', MOUSE_BUTTONS_THAT_WORK)
def test_trigger_mouse_binding_with_modifier(hlwm, keyboard, mouse, button):
    hlwm.call('new_attr string my_press')
    hlwm.call(f'mousebind Mod1-Button{button} call set_attr my_press yup')
    hlwm.call(f'mousebind Button{button} call remove_attr my_press')  # canary bind (should not trigger)
    client_id, _ = hlwm.create_client()

    keyboard.down('Alt')
    mouse.click(str(button), client_id)
    keyboard.up('Alt')

    assert hlwm.get_attr('my_press') == 'yup'


def test_overlapping_bindings_most_recent_one_counts(hlwm, mouse):
    hlwm.call('new_attr string my_press')
    hlwm.call('mousebind Button2 call set_attr my_press firstbind')
    hlwm.call('mousebind Button2 call set_attr my_press secondbind')

    client_id, _ = hlwm.create_client()
    mouse.click('2', client_id)

    assert hlwm.get_attr('my_press') == 'secondbind'


@pytest.mark.parametrize('prefix', ['', 'Mod1+'])
def test_complete_mousebind_offers_all_mods_and_buttons(hlwm, prefix):
    complete = hlwm.complete(['mousebind', prefix], partial=True, position=1)

    buttons = sum(([f'Button{i}', f'B{i}'] for i in MOUSE_BUTTONS_THAT_EXIST), [])
    mods = ['Alt', 'Control', 'Ctrl', 'Mod1', 'Mod2', 'Mod3', 'Mod4', 'Mod5', 'Shift', 'Super']
    if prefix == 'Mod1+':
        mods = [m for m in mods if m not in ['Mod1', 'Alt']]
    assert sorted(c[:-1] for c in complete) == sorted(prefix + i for i in mods + buttons)


def test_complete_mousebind_after_button_offers_action(hlwm):
    complete = hlwm.complete('mousebind B3', partial=False, position=2)

    assert set(complete) == {'move', 'resize', 'zoom', 'call'}


def test_complete_mousebind_with_call_action_offers_all_commands(hlwm):
    complete = hlwm.complete('mousebind B1 call', position=3)

    assert complete == hlwm.complete('', position=0)


def test_complete_mousebind_validates_all_button(hlwm):
    # Note: This might seem like a stupid test, but previous implementations
    # ignored the invalid first modifier.
    complete = hlwm.complete('mousebind Moo+Mo', partial=True, position=1)

    assert complete == []


# we had a race condition here, so increase the likelyhood
# that we really fixed it:
@pytest.mark.parametrize('repeat', list(range(0, 100)))
def test_drag_move(hlwm, x11, mouse, repeat):
    hlwm.call('set_attr tags.focus.floating on')
    client, winid = x11.create_client()
    x, y = x11.get_absolute_top_left(client)
    mouse.move_into(winid, wait=True)

    hlwm.call(['drag', winid, 'move'])
    mouse.move_relative(12, 15)
    hlwm.call('true')  # sync

    assert x11.get_absolute_top_left(client) == (x + 12, y + 15)


def test_drag_no_frame_splits(hlwm):
    winid, _ = hlwm.create_client()

    hlwm.call_xfail(['drag', winid, 'resize']) \
        .expect_stderr('No neighbour frame')


def test_mouse_drag_no_frame_splits(hlwm, hlwm_process, mouse):
    hlwm.call('mousebind B1 resize')
    winid, _ = hlwm.create_client()

    with hlwm_process.wait_stderr_match('No neighbour frame'):
        # we do not wait because it clashes with the running
        # wait_stderr_match() context here
        mouse.click('1', winid, wait=False)


def test_drag_invisible_client(hlwm):
    # check that we can't resize clients that are on a tag
    # that is not shown
    hlwm.call('add t')
    hlwm.call('set_attr tags.by-name.t.floating on')

    # invisible win
    kid, _ = hlwm.create_client()
    # got a tag of his own
    hlwm.call('move t')
    # where he'll never be known
    hlwm.call_xfail(['drag', kid, 'resize']) \
        .expect_stderr('can not drag invisible client')
    # inward he's grown :-)


def test_drag_resize_tiled_client(hlwm, mouse):
    winid, _ = hlwm.create_client()
    layout = '(split horizontal:{}:1 (clients max:0) (clients max:0 {}))'
    hlwm.call(['load', layout.format('0.5', winid)])
    # Just positioning the mouse pointer, no need to wait for hlwm
    mouse.move_into(winid, x=10, y=30, wait=False)

    hlwm.call(['drag', winid, 'resize'])
    assert hlwm.get_attr('clients.dragged.winid') == winid
    mouse.move_relative(200, 300)

    monitor_width = int(hlwm.call('monitor_rect').stdout.split(' ')[2])
    layout_str = hlwm.call('dump').stdout
    layout_ma = re.match(layout.replace('(', r'\(')
                               .replace(')', r'\)')
                               .format('([^:]*)', '.*'), layout_str)
    expected = 0.5 + 200 / monitor_width
    actual = float(layout_ma.group(1))
    assert math.isclose(actual, expected, abs_tol=0.01)


@pytest.mark.parametrize('live_update', [True, False])
def test_drag_resize_floating_client(hlwm, x11, mouse, live_update):
    hlwm.call(['set', 'update_dragged_clients', hlwm.bool(live_update)])

    client, winid = x11.create_client(geometry=(50, 50, 300, 200))
    hlwm.call(f'set_attr clients.{winid}.floating true')
    geom_before = client.get_geometry()
    x_before, y_before = x11.get_absolute_top_left(client)
    assert (geom_before.width, geom_before.height) == (300, 200)
    # move cursor to the top left corner, so we change the
    # window position and the size (and the bottom right corner is fixed)
    # Just positioning the mouse pointer, no need to wait for hlwm
    mouse.move_into(winid, x=0, y=0, wait=False)

    hlwm.call(['drag', winid, 'resize'])
    assert hlwm.get_attr('clients.dragged.winid') == winid
    mouse.move_relative(100, 120)
    final_size = (geom_before.width - 100, geom_before.height - 120)

    # check geometry during drag
    x11.display.sync()
    geom_after = client.get_geometry()
    x_after, y_after = x11.get_absolute_top_left(client)
    assert (x_after, y_after) == (x_before + 100, y_before + 120)
    expected_size = (geom_before.width, geom_before.height)
    if live_update:
        expected_size = final_size
    assert (geom_after.width, geom_after.height) == expected_size

    # stop drag and check final size
    mouse.click('1', wait=True)
    geom_after = client.get_geometry()
    assert (geom_after.width, geom_after.height) == final_size


def test_drag_zoom_floating_client(hlwm, x11, mouse):
    client, winid = x11.create_client(geometry=(50, 50, 300, 200))
    hlwm.call(f'set_attr clients.{winid}.floating true')
    geom_before = client.get_geometry()
    assert (geom_before.width, geom_before.height) == (300, 200)
    x_before, y_before = x11.get_absolute_top_left(client)
    center_before = (x_before + geom_before.width / 2, y_before + geom_before.height / 2)
    # Just positioning the mouse pointer, no need to wait for hlwm
    mouse.move_into(winid, x=0, y=0, wait=False)

    hlwm.call(['drag', winid, 'zoom'])
    assert hlwm.get_attr('clients.dragged.winid') == winid
    mouse.move_relative(100, -30)
    final_size = (geom_before.width - (100 * 2), geom_before.height + (30 * 2))

    # stop drag and check final size and client center
    mouse.click('1', wait=True)
    geom_after = client.get_geometry()
    assert (geom_after.width, geom_after.height) == final_size
    x_after, y_after = x11.get_absolute_top_left(client)
    center_after = (x_after + geom_after.width / 2, y_after + geom_after.height / 2)
    assert center_before == center_after


# we had a race condition here, so increase the likelyhood
# that we really fixed it:
@pytest.mark.parametrize('repeat', list(range(0, 100)))
def test_move_client_via_decoration(hlwm, x11, mouse, repeat):
    hlwm.call('attr theme.padding_top 20')
    client, winid = x11.create_client(geometry=(50, 50, 300, 200))
    hlwm.call(f'set_attr clients.{winid}.floating true')
    size_before = client.get_geometry()
    x_before, y_before = x11.get_absolute_top_left(client)
    mouse.move_to(x_before + 10, y_before - 10)  # a bit into the padding

    mouse.mouse_press('1')
    assert hlwm.get_attr('clients.dragged.winid') == winid

    mouse.move_relative(130, 110)
    expected_position = (x_before + 130, y_before + 110)

    mouse.mouse_release('1')
    x11.display.sync()
    assert 'dragged' not in hlwm.list_children('clients')
    # the size didn't change
    size_after = client.get_geometry()
    assert (size_before.width, size_before.height) \
        == (size_after.width, size_after.height)
    # but the location
    assert expected_position == x11.get_absolute_top_left(client)


# we had a race condition here, so increase the likelyhood
# that we really fixed it:
@pytest.mark.parametrize('repeat', list(range(0, 100)))
def test_resize_client_via_decoration(hlwm, x11, mouse, repeat):
    hlwm.call('attr theme.border_width 20')
    client, winid = x11.create_client(geometry=(50, 50, 300, 200))
    hlwm.call(f'set_attr clients.{winid}.floating true')
    size_before = client.get_geometry()
    x_before, y_before = x11.get_absolute_top_left(client)
    mouse.move_to(x_before + 10, y_before - 10)  # a bit into the window border

    mouse.mouse_press('1')
    assert hlwm.get_attr('clients.dragged.winid') == winid

    mouse.move_relative(80, 70)
    expected_position = (x_before + 80, y_before + 70)
    expected_size = (size_before.width - 80, size_before.height - 70)

    mouse.mouse_release('1')

    # the size changed
    x11.display.sync()
    assert 'dragged' not in hlwm.list_children('clients')
    size_after = client.get_geometry()
    assert expected_size == (size_after.width, size_after.height)
    # and also the location
    assert expected_position == x11.get_absolute_top_left(client)
