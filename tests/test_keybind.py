import pytest
import subprocess


def test_list_keybinds(hlwm):
    hlwm.call('keybind Mod1-x quit')

    keybinds = hlwm.call('list_keybinds')

    assert keybinds.stdout == 'Mod1+x\tquit\n'


def test_keybind_unknown_modifier(hlwm):
    call = hlwm.call_xfail('keybind Moep1-x quit')

    assert call.stderr == 'keybind: No such KeySym/modifier\n'


def test_keybind_unknown_keysym(hlwm):
    call = hlwm.call_xfail('keybind Mod1-_ quit')

    assert call.stderr == 'keybind: No such KeySym/modifier\n'


def test_trigger_single_key_binding(hlwm):
    hlwm.call('keybind x use tag2')
    hlwm.call('add tag2')
    assert hlwm.get_attr('monitors.0.tag') != 'tag2'

    subprocess.call('xdotool key x'.split())

    assert hlwm.get_attr('monitors.0.tag') == 'tag2'
