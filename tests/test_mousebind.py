import pytest
import math
from herbstluftwm.types import Point

# Note: For unknown reasons, mouse buttons 4 and 5 (scroll wheel) do not work
# in Xvfb when running tests in the CI. Therefore, we maintain two lists of
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
    call.expect_stderr('Unknown mouse button "Button42"')


def test_mousebind_unknown_action(hlwm):
    call = hlwm.call_xfail('mousebind Button1 get schwifty')
    call.expect_stderr('Unknown mouse action "get"')


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
    r = hlwm.attr.clients[winid].floating_geometry()
    assert (r.x, r.y) == (x + 12, y + 15)


@pytest.mark.parametrize('update_dragged', [True, False])
def test_drag_move_sends_configure(hlwm, x11, mouse, update_dragged):
    hlwm.attr.tags.focus.floating = 'on'
    hlwm.attr.settings.update_dragged_clients = hlwm.bool(update_dragged)
    client, winid = x11.create_client()
    x, y = x11.get_absolute_top_left(client)
    before = hlwm.attr.clients[winid].content_geometry()
    assert (x, y) == (before.x, before.y)
    mouse.move_into(winid, wait=True)

    hlwm.call(['drag', winid, 'move'])
    mouse.move_relative(12, 15)
    mouse.click('1')  # stop dragging
    hlwm.call('true')  # sync

    after = hlwm.attr.clients[winid].content_geometry()
    assert before.adjusted(dx=12, dy=15) == after


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
        .expect_stderr('cannot drag invisible client')
    # inward he's grown :-)


def test_drag_minimized_client(hlwm):
    winid, _ = hlwm.create_client()
    hlwm.call(f'set_attr clients.{winid}.minimized on')

    hlwm.call_xfail(['drag', winid, 'resize']) \
        .expect_stderr('cannot drag invisible client')


@pytest.mark.parametrize('align', ['horizontal', 'vertical'])
def test_drag_resize_tiled_client(hlwm, mouse, align):
    winid, _ = hlwm.create_client()
    layout = '(split {}:{}:1 (clients max:0) (clients max:0 {}))'
    hlwm.call(['load', layout.format(align, '0.5', winid)])
    # Just positioning the mouse pointer, no need to wait for hlwm
    mouse.move_into(winid, x=10, y=30, wait=False)

    hlwm.call(['drag', winid, 'resize'])
    assert hlwm.get_attr('clients.dragged.winid') == winid
    delta = Point(200, 150)
    mouse.move_relative(delta.x, delta.y)

    monitor_geo = hlwm.attr.monitors.focus.geometry()
    if align == 'horizontal':
        expected = 0.5 + delta.x / monitor_geo.width
    else:
        expected = 0.5 + delta.y / monitor_geo.height
    actual = float(hlwm.attr.tags.focus.tiling.root.fraction())
    assert math.isclose(actual, expected, abs_tol=0.01)


@pytest.mark.parametrize('dir1', ['left', 'right'])
@pytest.mark.parametrize('dir2', ['top', 'bottom'])
def test_drag_resize_tiled_client_in_two_directions(hlwm, mouse, dir1, dir2):
    winid, _ = hlwm.create_client()
    # create two splits, keeping the client focused:
    hlwm.call(['split', dir1])
    # split the client again:
    hlwm.call(['split', dir2])

    # dir1 and dir2 implicitly define which corner of the frame is dragged
    def dragged_corner(rectangle):
        x = rectangle.x
        y = rectangle.y
        if dir1 == 'right':
            x += rectangle.width
        if dir2 == 'bottom':
            y += rectangle.height
        return Point(x, y)

    geo_before = hlwm.attr.clients.focus.decoration_geometry()
    corner_before = dragged_corner(geo_before)
    # Just move the mouse pointer into the interesting corner.
    # "into" means that the cursor is not on the corner but slightly
    # moved to the center of the window:
    cursor_rel = (corner_before * 9 + geo_before.center()) // 10 - geo_before.topleft()
    mouse.move_into(winid, x=cursor_rel.x, y=cursor_rel.y, wait=False)

    hlwm.call(['drag', winid, 'resize'])
    assert hlwm.get_attr('clients.dragged.winid') == winid

    delta = Point(80, 70)
    mouse.move_relative(delta.x, delta.y)

    geo_after = hlwm.attr.clients.focus.decoration_geometry()
    corner_after = dragged_corner(geo_after)
    corner_expected = corner_before + delta
    assert math.isclose(corner_expected.x, corner_after.x, abs_tol=0.01)
    assert math.isclose(corner_expected.y, corner_after.y, abs_tol=0.01)


def test_client_resize_does_not_switch_tabs(hlwm, mouse):
    bw = 5
    hlwm.attr.theme.border_width = bw
    hlwm.attr.theme.title_height = 20
    winids = hlwm.create_clients(2)
    winids_str = ' '.join(winids)
    layout = f'(split horizontal:0.5:0 (clients max:0 {winids_str}) (clients vertical:0))'
    hlwm.call(['load', layout])

    client_geo = hlwm.attr.clients[winids[0]].decoration_geometry()
    # clicking the top right corner does not switch the tabs:
    mouse.move_to(client_geo.x + client_geo.width - bw // 2, client_geo.y + bw // 2)
    mouse.mouse_press('1')

    assert hlwm.attr.clients.dragged.winid() == winids[0]
    assert hlwm.attr.clients.focus.winid() == winids[0]

    # double check, that the tab was indeed just nearby:
    mouse.mouse_release('1')
    mouse.move_relative(-bw, bw)
    mouse.click('1')
    assert hlwm.attr.clients.focus.winid() == winids[1]


@pytest.mark.parametrize('live_update', [True, False])
def test_drag_resize_floating_client(hlwm, x11, mouse, live_update):
    hlwm.attr.settings.update_dragged_clients = hlwm.bool(live_update)

    client, winid = x11.create_client(geometry=(50, 50, 300, 200))
    hlwm.call(f'set_attr clients.{winid}.floating true')
    geom_before = hlwm.attr.clients[winid].content_geometry()
    assert geom_before == x11.get_absolute_geometry(client)  # duck-typing
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
    assert (x_after, y_after) == (geom_before.x + 100, geom_before.y + 120)
    expected_size = (geom_before.width, geom_before.height)
    if live_update:
        expected_size = final_size
    assert (geom_after.width, geom_after.height) == expected_size

    # stop drag and check final size
    mouse.click('1', wait=True)
    geom_after = client.get_geometry()
    assert (geom_after.width, geom_after.height) == final_size
    assert hlwm.attr.clients[winid].content_geometry() \
        == geom_before.adjusted(dx=100, dy=120, dw=-100, dh=-120)
    assert hlwm.attr.clients[winid].floating_geometry() \
        == geom_before.adjusted(dx=100, dy=120, dw=-100, dh=-120)


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


@pytest.mark.parametrize('floating', [True, False])
def test_resize_unfocused_client(hlwm, mouse, floating):
    hlwm.attr.theme.border_width = 5
    winid, _ = hlwm.create_client()
    focus, _ = hlwm.create_client()
    # place clients side by side, with the focus on the left
    layout = f'(split horizontal:0.5:0 (clients vertical:0 {focus}) (clients vertical:0 {winid}))'
    hlwm.call(['load', layout])
    if floating:
        # steal the tiling layout to the floating geometries:
        hlwm.attr.clients[winid].floating_geometry = hlwm.attr.clients[winid].content_geometry()
        hlwm.attr.clients[focus].floating_geometry = hlwm.attr.clients[focus].content_geometry()
        hlwm.attr.tags.focus.floating = True
        hlwm.attr.clients[winid].sizehints_floating = False
        hlwm.attr.settings.snap_distance = 0

    assert hlwm.attr.clients.focus.winid() == focus

    geo = hlwm.attr.clients[winid].decoration_geometry()
    # move the cursor into the left border of the right hand client:
    mouse.move_to(geo.x + 2, geo.y + geo.height // 2)
    mouse.mouse_press('1')
    assert hlwm.attr.clients.focus.winid() == focus
    assert hlwm.attr.clients.dragged.winid() == winid

    dx = 17
    mouse.move_relative(-dx, -20)
    mouse.mouse_release('1')
    assert 'dragged' not in hlwm.list_children('clients')
    new_geo = hlwm.attr.clients[winid].decoration_geometry()
    # the window grew 13 pixels to the left:
    assert new_geo.x == geo.x - dx
    assert new_geo.width == geo.width + dx
    assert hlwm.attr.clients.focus.winid() == focus, \
        "the focus didn't change"
    fraction_attr = hlwm.attr.tags.focus.tiling.root.fraction
    if floating:
        assert fraction_attr() == '0.5', \
            "the tiling shouldn't change in floating resize"
    else:
        assert fraction_attr() != '0.5', \
            "the tiling layout is updated"


@pytest.mark.parametrize('resize_possible', [True, False])
def test_border_click_either_focuses_or_resizez(hlwm, mouse, resize_possible):
    """
    when clicking on the outer decoration of a window, then this should either
    trigger the resize or focus the client (if resizing is not possible).
    """
    hlwm.attr.theme.border_width = 5
    hlwm.attr.settings.focus_follows_mouse = False
    target, _ = hlwm.create_client()
    focus, _ = hlwm.create_client()
    if resize_possible:
        # place clients below each other, with focus on the top
        split_align = 'vertical'
    else:
        # place clients side by side, with the focus on the left
        split_align = 'horizontal'
    layout = f'(split {split_align}:0.5:0 (clients vertical:0 {focus}) (clients vertical:0 {target}))'
    hlwm.call(['load', layout])
    assert hlwm.attr.clients.focus.winid() == focus

    target_geo = hlwm.attr.clients[target].decoration_geometry()
    # click into the top of the decoration of the unfocused window.
    click_point = target_geo.topleft() + Point(target_geo.width // 2, 3)
    mouse.move_to(click_point.x, click_point.y)
    mouse.mouse_press('1')

    # this either starts dragging or focuses the 'target' window
    focus_change = not resize_possible
    assert ('dragged' in hlwm.list_children('clients')) == resize_possible
    assert (hlwm.attr.clients.focus.winid() == target) == focus_change


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
