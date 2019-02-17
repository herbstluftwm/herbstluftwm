import pytest

# Note: Actually, buttons 4 and 5 (scroll wheel) should also be tested. But for
# some unknown reason, those don't work in Xvfb when running tests on Travis,
# so they are not included here.
MOUSE_BUTTONS = [1, 2, 3]


@pytest.mark.parametrize('method', ['-F', '--all'])
def test_mouseunbind_all(hlwm, method, mouse):
    hlwm.create_client()
    hlwm.call('mousebind Button1 call quit')

    unbind = hlwm.call(['mouseunbind', method])

    assert unbind.stdout == ''
    # TODO: assert hlwm.call('list_mousebind').stdout == ''
    mouse.click('1')  # verify that binding got ungrabbed


@pytest.mark.parametrize('button', MOUSE_BUTTONS)
def test_trigger_mouse_binding_without_modifier(hlwm, mouse, button):
    hlwm.call('new_attr string my_press')
    hlwm.call(f'mousebind Button{button} call set_attr my_press yup')
    client_id, _ = hlwm.create_client()

    mouse.click(str(button), client_id)

    assert hlwm.get_attr('my_press') == 'yup'


@pytest.mark.parametrize('button', MOUSE_BUTTONS)
def test_trigger_mouse_binding_with_modifier(hlwm, keyboard, mouse, button):
    hlwm.call('new_attr string my_press')
    hlwm.call(f'mousebind Mod1-Button{button} call set_attr my_press yup')
    hlwm.call(f'mousebind Button{button} call remove_attr my_press')  # canary bind (should not trigger)
    client_id, _ = hlwm.create_client()

    keyboard.down('Alt')
    mouse.click(str(button), client_id)
    keyboard.up('Alt')

    assert hlwm.get_attr('my_press') == 'yup'
