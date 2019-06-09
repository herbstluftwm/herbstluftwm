import pytest

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


def test_complete_mousebind_offers_all_mods_and_buttons(hlwm):
    complete = hlwm.complete('mousebind', partial=True, position=1)

    buttons = sum(([f'Button{i}', f'B{i}'] for i in MOUSE_BUTTONS_THAT_EXIST), [])
    mods = ['Alt', 'Control', 'Ctrl', 'Mod1', 'Mod2', 'Mod3', 'Mod4', 'Mod5', 'Shift', 'Super']
    assert sorted(c[:-1] for c in complete) == sorted(mods + buttons)


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
