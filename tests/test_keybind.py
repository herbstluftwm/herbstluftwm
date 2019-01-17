import pytest
import subprocess


@pytest.mark.parametrize('separator', ['-', '+'])
def test_list_keybinds(hlwm, separator):
    hlwm.call(f'keybind Mod1{separator}x quit')

    keybinds = hlwm.call('list_keybinds')

    assert keybinds.stdout == 'Mod1+x\tquit\n'


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


def test_trigger_single_key_binding(hlwm):
    hlwm.call('keybind x use tag2')
    hlwm.call('add tag2')
    assert hlwm.get_attr('monitors.0.tag') != 'tag2'

    subprocess.call('xdotool key x'.split())

    assert hlwm.get_attr('monitors.0.tag') == 'tag2'
