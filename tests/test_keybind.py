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


def test_keybind_unknown_modifier(hlwm):
    call = hlwm.call_xfail('keybind Moep1-x quit')

    assert call.stderr == 'keybind: No such KeySym/modifier\n'


def test_keybind_unknown_keysym(hlwm):
    call = hlwm.call_xfail('keybind Mod1-_ quit')

    assert call.stderr == 'keybind: No such KeySym/modifier\n'


def test_replace_keybind(hlwm):
    hlwm.call(f'keybind Mod1+x quit')

    hlwm.call(f'keybind Mod1+x cycle')

    assert hlwm.call('list_keybinds').stdout == 'Mod1+x\tcycle\n'


def test_keyunbind_specific_binding(hlwm):
    hlwm.call('keybind Mod1+x quit')
    hlwm.call('keybind Mod1+y cycle')
    hlwm.call('keybind Mod1+z close')

    unbind = hlwm.call('keyunbind Mod1+y')

    assert unbind.stdout == ''
    assert hlwm.call('list_keybinds').stdout == 'Mod1+x\tquit\nMod1+z\tclose\n'


@pytest.mark.parametrize('method', ['-F', '--all'])
def test_keyunbind_all(hlwm, method):
    hlwm.call('keybind Mod1+x quit')

    unbind = hlwm.call(['keyunbind', method])

    assert unbind.stdout == ''
    assert hlwm.call('list_keybinds').stdout == ''


def test_keyunbind_nonexistent_binding(hlwm):
    unbind = hlwm.call('keyunbind n')

    assert unbind.stdout == 'keyunbind: Key "n" is not bound\n'


def test_trigger_single_key_binding(hlwm):
    hlwm.call('keybind x use tag2')
    hlwm.call('add tag2')
    assert hlwm.get_attr('monitors.0.tag') != 'tag2'

    subprocess.call('xdotool key x'.split())

    assert hlwm.get_attr('monitors.0.tag') == 'tag2'


def test_trigger_selfremoving_binding(hlwm):
    hlwm.call('keybind x keyunbind x')

    subprocess.call('xdotool key x'.split())

    assert hlwm.call('list_keybinds').stdout == ''


def test_complete_keybind_offers_all_mods_and_syms(hlwm):
    complete = hlwm.complete('keybind', partial=True, position=1)

    assert len(complete) > 200  # plausibility check
    assert sorted([c[:-1] for c in complete if c.endswith('+')]) == \
        ['Alt', 'Control', 'Ctrl', 'Mod1', 'Mod2', 'Mod3', 'Mod4', 'Mod5', 'Shift', 'Super']


def test_complete_keybind_after_combo_offers_all_commands(hlwm):
    complete = hlwm.complete('keybind x', position=2)

    assert complete == hlwm.complete('', position=0)
