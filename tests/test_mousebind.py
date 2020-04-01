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
    hlwm.call(f'mousebind Button2 call set_attr my_press firstbind')
    hlwm.call(f'mousebind Button2 call set_attr my_press secondbind')

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
    mouse.move_into(winid)

    hlwm.call(['drag', winid, 'move'])
    mouse.move_relative(12, 15)
    hlwm.call('true')  # sync

    assert x11.get_absolute_top_left(client) == (x + 12, y + 15)


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
    mouse.move_into(winid, x=10, y=30)

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
