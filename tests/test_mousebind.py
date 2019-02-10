import pytest


@pytest.mark.parametrize('method', ['-F', '--all'])
def test_mouseunbind_all(hlwm, method, mouse):
    hlwm.create_client()
    hlwm.call('mousebind Button1 call quit')

    unbind = hlwm.call(['mouseunbind', method])

    assert unbind.stdout == ''
    # TODO: assert hlwm.call('list_mousebind').stdout == ''
    mouse.click('1')  # verify that binding got ungrabbed


@pytest.mark.parametrize('button', [1, 2, 3, 4, 5])
def test_trigger_mouse_binding_without_modifier(hlwm, mouse, button):
    hlwm.call(f'mousebind Button{button} call close')
    client_id, _ = hlwm.create_client()

    mouse.click(str(button), client_id)

    assert hlwm.get_attr('tags.0.client_count') == '0'


@pytest.mark.parametrize('button', [1, 2, 3, 4, 5])
def test_trigger_mouse_binding_with_modifier(hlwm, keyboard, mouse, button):
    hlwm.call(f'mousebind Mod1-Button{button} call close')
    client_id, _ = hlwm.create_client()

    keyboard.down('Alt')
    mouse.click(str(button), client_id)
    keyboard.up('Alt')

    assert hlwm.get_attr('tags.0.client_count') == '0'
