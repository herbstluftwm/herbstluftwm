import pytest
import subprocess


@pytest.mark.parametrize('sep', ['-', '+'])
def test_list_keybinds(hlwm, sep):
    # single key, 1-word command:
    hlwm.call(f'keybind x quit')
    # 2 modifiers, 3-word command:
    hlwm.call(f'keybind Mod1{sep}Shift{sep}a resize left +5')

    keybinds = hlwm.call('list_keybinds')

    assert keybinds.stdout == 'x\tquit\nMod1+Shift+a\tresize\tleft\t+5\n'


@pytest.mark.parametrize('combo,message', [
    ('Moep1-x', 'keybind: Unknown modifier "Moep1"'),
    ('Mod1-_', 'keybind: Unknown KeySym "_"'),
    ('', 'keybind: Must not be empty'),
])
def test_keybind_invalid_key_combo(hlwm, combo, message):
    call = hlwm.call_xfail(['keybind', combo, 'quit'])
    call.expect_stderr(message)


def test_replace_keybind(hlwm):
    hlwm.call(f'keybind Mod1+x quit')

    hlwm.call(f'keybind Mod1+x cycle')

    assert hlwm.call('list_keybinds').stdout == 'Mod1+x\tcycle\n'


def test_keyunbind_specific_binding(hlwm, keyboard):
    hlwm.call('keybind Mod1+x cycle')
    hlwm.call('keybind Ctrl+y quit')
    hlwm.call('keybind Mod1+z close')

    unbind = hlwm.call('keyunbind Ctrl+y')

    assert unbind.stdout == ''
    assert hlwm.call('list_keybinds').stdout == 'Mod1+x\tcycle\nMod1+z\tclose\n'
    keyboard.press('Ctrl+y')  # verify that key got ungrabbed


@pytest.mark.parametrize('method', ['-F', '--all'])
def test_keyunbind_all(hlwm, method, keyboard):
    hlwm.call('keybind Alt+x quit')

    unbind = hlwm.call(['keyunbind', method])

    assert unbind.stdout == ''
    assert hlwm.call('list_keybinds').stdout == ''
    keyboard.press('Alt+x')  # verify that key got ungrabbed


def test_keyunbind_nonexistent_binding(hlwm):
    unbind = hlwm.call('keyunbind n')

    assert unbind.stdout == 'keyunbind: Key "n" is not bound\n'


def test_trigger_single_key_binding(hlwm, keyboard):
    hlwm.call('keybind x use tag2')
    hlwm.call('add tag2')
    assert hlwm.get_attr('monitors.0.tag') != 'tag2'

    keyboard.press('x')

    assert hlwm.get_attr('monitors.0.tag') == 'tag2'


def test_trigger_selfremoving_binding(hlwm, keyboard):
    hlwm.call('keybind x keyunbind x')

    keyboard.press('x')

    assert hlwm.call('list_keybinds').stdout == ''


@pytest.mark.parametrize('maskmethod', ('rule', 'set_attr'))  # how keys_inactive gets set
@pytest.mark.parametrize('whenbind', ('existing', 'added_later'))  # when keybinding is set up
@pytest.mark.parametrize('refocus', (True, False))  # whether to defocus+refocus before keypress
def test_keys_inactive(hlwm, keyboard, maskmethod, whenbind, refocus):
    if whenbind == 'existing':
        hlwm.call('keybind x add tag2')
    if maskmethod == 'rule':
        hlwm.call('rule once keys_inactive=^x$')

    _, client_proc = hlwm.create_client(term_command='read -n 1')

    if maskmethod == 'set_attr':
        hlwm.call('set_attr clients.focus.keys_inactive ^x$')
    if whenbind == 'added_later':
        hlwm.call('keybind x add tag2')

    if refocus:
        hlwm.create_client()
        hlwm.call('cycle +1')
        hlwm.call('cycle -1')

    keyboard.press('x')

    # Expect client to have quit because of received keypress:
    try:
        print(f"waiting for client proc {client_proc.pid}")
        client_proc.wait(5)
    except subprocess.TimeoutExpired:
        assert False, "Expected client to quit, but it is still running"

    # As a verification of the test itself, check that the keybinding was not
    # triggered:
    hlwm.call_xfail('attr tags.1')


@pytest.mark.parametrize('maskmethod', ('rule', 'set_attr'))
def test_invalid_keys_inactive(hlwm, keyboard, maskmethod):
    hlwm.call('keybind x close')
    if maskmethod == 'rule':
        hlwm.call('rule once keys_inactive=[b-a]')
    hlwm.create_client()
    if maskmethod == 'set_attr':
        # Note: In future work, we could make this fail right away. But
        # currently, that is not the case.
        hlwm.call('set_attr clients.focus.keys_inactive [b-a]')

    keyboard.press('x')

    assert hlwm.get_attr('tags.0.client_count') == '0'


@pytest.mark.parametrize('prefix', ['', 'Mod1+'])
def test_complete_keybind_offers_all_mods_and_syms(hlwm, prefix):
    complete = hlwm.complete(['keybind', prefix], partial=True, position=1)

    assert len(complete) > 200  # plausibility check
    all_mods = ['Alt', 'Control', 'Ctrl', 'Mod1', 'Mod2', 'Mod3', 'Mod4', 'Mod5', 'Shift', 'Super']
    if prefix == 'Mod1+':
        all_mods = [m for m in all_mods if m not in ['Mod1', 'Alt']]
    assert sorted([c[:-1] for c in complete if c.endswith('+')]) == \
        sorted([prefix + m for m in all_mods])


def test_complete_keybind_after_combo_offers_all_commands(hlwm):
    complete = hlwm.complete('keybind x', position=2)

    assert complete == hlwm.complete('', position=0)


def test_complete_keybind_offers_additional_mods_without_duplication(hlwm):
    complete = hlwm.complete('keybind Mod2+Mo', partial=True, position=1)

    assert set(complete) == {
        'Mod2+Mod1+',
        'Mod2+Mod3+',
        'Mod2+Mod4+',
        'Mod2+Mod5+',
        'Mod2+Mode_switch ',
    }


def test_complete_keybind_validates_all_tokens(hlwm):
    # Note: This might seem like a stupid test, but previous implementations
    # ignored the invalid first modifier.
    complete = hlwm.complete('keybind Moo+Mo', partial=True, position=1)

    assert complete == []


def test_keys_inactive_on_other_client(hlwm, keyboard):
    c1, _ = hlwm.create_client()
    c2, _ = hlwm.create_client()
    hlwm.call('keybind x set_attr clients.focus.pseudotile on')
    hlwm.call(f'set_attr clients.{c1}.keys_inactive x')
    hlwm.call(f'jumpto {c1}')

    hlwm.call(f'jumpto {c2}')
    keyboard.press('x')

    assert hlwm.get_attr('clients.focus.pseudotile') == 'true'
