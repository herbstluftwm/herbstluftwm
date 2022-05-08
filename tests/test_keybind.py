import pytest
import subprocess

from conftest import PROCESS_SHUTDOWN_TIME


@pytest.mark.parametrize('sep', ['-', '+'])
def test_list_keybinds(hlwm, sep):
    # single key, 1-word command:
    hlwm.call('keybind x quit')
    # 2 modifiers, 3-word command:
    hlwm.call(f'keybind Mod1{sep}Shift{sep}a resize left +5')

    keybinds = hlwm.call('list_keybinds')

    assert keybinds.stdout == 'x\tquit\nMod1+Shift+a\tresize\tleft\t+5\n'


@pytest.mark.parametrize('combo,message', [
    ('Moep1-x', 'keybind:.* Unknown modifier "Moep1"'),
    ('Mod1-_', 'keybind:.* Unknown KeySym "_"'),
    ('', 'keybind:.* Must not be empty'),
])
def test_keybind_invalid_key_combo(hlwm, combo, message):
    call = hlwm.call_xfail(['keybind', combo, 'quit'])
    call.expect_stderr(message)


def test_replace_keybind(hlwm):
    hlwm.call('keybind Mod1+x quit')

    hlwm.call('keybind Mod1+x cycle')

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
    hlwm.call_xfail('keyunbind n') \
        .expect_stderr('keyunbind: Key "n" is not bound\n')


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

    winid, client_proc = hlwm.create_client(term_command='read -n 1')

    if maskmethod == 'set_attr':
        hlwm.call('set_attr clients.focus.keys_inactive ^x$')
    if whenbind == 'added_later':
        hlwm.call('keybind x add tag2')

    if refocus:
        hlwm.create_client()
        hlwm.call('cycle +1')
        hlwm.call('cycle -1')

    # it may take a bit until the client acquires the input focus
    keyboard.wait_until_window_is_focused(winid)
    keyboard.press('x')

    # we expect that the keybind command is not executed:
    assert hlwm.list_children('tags.by-name.') == ['default']
    # instead, the client must quit because of received keypress:
    try:
        print(f"waiting for client proc {client_proc.pid}")
        client_proc.wait(PROCESS_SHUTDOWN_TIME)
    except subprocess.TimeoutExpired:
        assert False, "Expected client to quit, but it is still running"

    # As a verification of the test itself, check that the keybinding was not
    # triggered:
    hlwm.call_xfail('attr tags.1')


def test_keyunbind_makes_client_receive_event(hlwm, keyboard):
    hlwm.call(['keybind', 'z', 'echo', 'foo'])
    winid, client_proc = hlwm.create_client(term_command='read -n 1')
    # remove the binding after the client was created
    hlwm.call(['keyunbind', 'z'])

    assert hlwm.attr.clients.focus.winid() == winid

    keyboard.press('z')

    try:
        print(f"waiting for client proc {client_proc.pid}")
        client_proc.wait(PROCESS_SHUTDOWN_TIME)
    except subprocess.TimeoutExpired:
        assert False, "Expected client to quit, but it is still running"


def test_invalid_keys_inactive_via_rule(hlwm, keyboard):
    hlwm.call_xfail('rule once keys_inactive=[b-a]') \
        .expect_stderr(r'Invalid .*\[b-a\].*:.*Invalid range in bracket')


@pytest.mark.parametrize('prefix', ['', 'Mod1+'])
def test_complete_keybind_offers_all_mods_and_syms(hlwm, prefix):
    complete = hlwm.complete(['keybind', prefix], partial=True, position=1)

    assert len(complete) > 200  # plausibility check
    all_mods = ['Alt', 'Control', 'Ctrl', 'Mod1', 'Mod2', 'Mod3', 'Mod4', 'Mod5', 'Shift', 'Super']
    if prefix == 'Mod1+':
        all_mods = [m for m in all_mods if m not in ['Mod1', 'Alt']]
    else:
        # if the prefix is not 'M...', then the completion suggests 'Release..'
        all_mods.append('Release')
    assert sorted([c[:-1] for c in complete if c.endswith('+')]) == \
        sorted([prefix + m for m in all_mods])


def test_complete_keybind_after_combo_offers_all_commands(hlwm):
    complete = hlwm.complete('keybind x', position=2)

    assert complete == hlwm.complete('', position=0)


def test_keys_inactive_regrab_all(hlwm, keyboard):
    hlwm.create_client()
    hlwm.call('new_attr string my_x_pressed')
    hlwm.call('keybind x set_attr my_x_pressed pressed')
    hlwm.call('keybind y true')
    # this disables the x binding
    hlwm.call('set_attr clients.focus.keys_inactive x')

    hlwm.call('keyunbind y')
    keyboard.press('x')

    # check that x is really disabled:
    assert hlwm.get_attr('my_x_pressed') == ''


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


@pytest.mark.parametrize('via_rule', [False, True])
def test_keymask_for_existing_binds(hlwm, keyboard, via_rule):
    hlwm.call('keybind x set_attr my_x_pressed pressed')
    hlwm.call('keybind y set_attr my_y_pressed pressed')
    if via_rule:
        hlwm.call('rule keymask=x')
    winid, _ = hlwm.create_client()
    if not via_rule:
        hlwm.call('set_attr clients.focus.keymask x')
    assert hlwm.get_attr('clients.focus.keymask') == 'x'
    hlwm.call('new_attr string my_x_pressed')
    hlwm.call('new_attr string my_y_pressed')

    keyboard.wait_until_window_is_focused(winid)
    # y does not match the mask, thus is not allowed
    keyboard.press('x')
    keyboard.press('y')

    assert hlwm.get_attr('my_x_pressed') == 'pressed'
    assert hlwm.get_attr('my_y_pressed') == ''


def test_keymask_applied_to_new_binds(hlwm, keyboard):
    winid, _ = hlwm.create_client()
    hlwm.create_client()  # another client
    keyboard.wait_until_window_is_focused(winid)
    assert hlwm.attr.clients.focus.winid() == winid
    hlwm.call('new_attr string my_x_pressed')
    hlwm.call('new_attr string my_y_pressed')
    hlwm.call('set_attr clients.focus.keymask y')

    hlwm.call('keybind x set_attr my_x_pressed pressed')
    hlwm.call('keybind y set_attr my_y_pressed pressed')

    keyboard.press('x')
    keyboard.press('y')

    assert hlwm.get_attr('my_x_pressed') == ''
    assert hlwm.get_attr('my_y_pressed') == 'pressed'


def test_keymask_prefix(hlwm, keyboard):
    hlwm.call('keybind space set_attr clients.focus.my_space_pressed pressed')
    hlwm.create_client()
    keyboard.wait_until_window_is_focused(hlwm.attr.clients.focus.winid())
    hlwm.call('set_attr clients.focus.keymask s')
    hlwm.call('new_attr string clients.focus.my_space_pressed')

    # according to the keymask, s is allowed, space is not
    keyboard.press('space')

    assert hlwm.get_attr('clients.focus.my_space_pressed') == ''


def test_keys_inactive_on_other_client(hlwm, keyboard):
    c1, _ = hlwm.create_client()
    c2, _ = hlwm.create_client()
    hlwm.call('keybind x set_attr clients.focus.pseudotile on')
    hlwm.call(f'set_attr clients.{c1}.keys_inactive x')
    hlwm.call(f'jumpto {c1}')

    hlwm.call(f'jumpto {c2}')
    keyboard.wait_until_window_is_focused(c2)
    keyboard.press('x')

    assert hlwm.get_attr('clients.focus.pseudotile') == 'true'


def test_keymask_type(hlwm):
    hlwm.create_client()
    hlwm.call(['set_attr',
               'clients.focus.keymask',
               r'Foo\(bar(a paren[thesis]*)* group'])
    hlwm.call(['set_attr',
               'clients.focus.keys_inactive',
               r'Foo\(bar(a paren[thesis]*)* group'])


@pytest.mark.parametrize('attribute', ['keymask', 'keys_inactive'])
def test_regex_syntax_error(hlwm, attribute):
    hlwm.create_client()
    hlwm.call_xfail(['set_attr', 'clients.focus.' + attribute, '(unmatch']) \
        .expect_stderr('not a valid value')
    hlwm.call_xfail(['set_attr', 'clients.focus.' + attribute, '[unmatch']) \
        .expect_stderr('not a valid value')
    hlwm.call_xfail(['set_attr', 'clients.focus.' + attribute, '[b-a]']) \
        .expect_stderr('not a valid value')


def test_keymask_complete(hlwm):
    hlwm.create_client()
    cmd = ['set_attr', 'clients.focus.keymask']
    reg = 'Foo(a [th]*)*'
    hlwm.call(cmd + [reg])
    assert hlwm.complete(cmd, evaluate_escapes=True) == [reg]


def test_keys_inactive_not_if_no_focus(hlwm, keyboard):
    hlwm.create_client()
    hlwm.call('set_attr clients.focus.keys_inactive x')
    hlwm.call('new_attr string my_x_pressed')
    hlwm.call('keybind x set_attr my_x_pressed pressed')
    hlwm.call('split h 0.5')
    hlwm.call('focus right')
    assert 'focus' not in hlwm.list_children('clients')

    keyboard.press('x')

    assert hlwm.get_attr('my_x_pressed') == 'pressed'


def test_keymask_and_keys_inactive(hlwm, keyboard):
    hlwm.create_client()
    hlwm.call(['set_attr', 'clients.focus.keymask', '[a-e]'])
    hlwm.call(['set_attr', 'clients.focus.keys_inactive', '[c-x]'])
    for k in ['a', 'c', 'f', 'z']:
        hlwm.call(f'new_attr string my_{k}_pressed')
        hlwm.call(f'keybind {k} set_attr my_{k}_pressed pressed')

    for k in ['a', 'c', 'f', 'z']:
        keyboard.press(k)

    # a is allowed by keymask and not disabled by keys_inactive
    assert hlwm.get_attr('my_a_pressed') == 'pressed'
    # c is allowed by keymask and but disabled by keys_inactive
    assert hlwm.get_attr('my_c_pressed') == ''
    # f is disallowed by keymask and disabled by keys_inactive
    assert hlwm.get_attr('my_f_pressed') == ''
    # z is disallowed by keymask but not disabled by keys_inactive
    assert hlwm.get_attr('my_z_pressed') == ''


def test_keybind_unknown_binding(hlwm):
    hlwm.call_xfail('keybind x xterm') \
        .expect_stderr('command.*not exist')


@pytest.mark.parametrize('command', ['keybind', 'keyunbind'])
def test_empty_keysym(hlwm, command):
    hlwm.call_xfail([command, '', 'true']) \
        .expect_stderr('Must not be empty')


def test_keybind_triggers_only_once(hlwm, keyboard):
    hlwm.call(['new_attr', 'int', 'my_test', '0'])
    hlwm.call(['keybind', 'x', 'set_attr', 'my_test', '+=1'])

    keyboard.down('x')
    assert hlwm.attr.my_test() == 1

    keyboard.up('x')
    assert hlwm.attr.my_test() == 1

    keyboard.down('x')
    assert hlwm.attr.my_test() == 2

    keyboard.up('x')
    assert hlwm.attr.my_test() == 2


def test_key_release_works_after_bind(hlwm, keyboard):
    hlwm.call(['new_attr', 'string', 'my_test', 'initial'])
    hlwm.call(['keybind', 'Release+x', 'set_attr', 'my_test', 'release'])

    keyboard.down('x')
    assert hlwm.attr.my_test() == 'initial'

    keyboard.up('x')
    assert hlwm.attr.my_test() == 'release'


def test_key_release_and_press(hlwm, keyboard):
    hlwm.call(['new_attr', 'string', 'my_test', 'initial'])
    hlwm.call(['keybind', 'Mod1-x', 'set_attr', 'my_test', 'press'])
    hlwm.call(['keybind', 'Release-Mod1-x', 'set_attr', 'my_test', 'release'])
    assert hlwm.attr.my_test() == 'initial'

    keyboard.down('alt+x')
    assert hlwm.attr.my_test() == 'press'

    # the modifier 'alt' still needs to be pressed during the following event:
    keyboard.up('x')
    assert hlwm.attr.my_test() == 'release'


def test_keys_inactive_key_can_be_removed(hlwm):
    hlwm.call(['keybind', 'x', 'echo', 'foo'])
    winid, _ = hlwm.create_client()
    hlwm.attr.clients[winid].keys_inactive = 'x'

    hlwm.call(['keyunbind', 'x'])
    assert hlwm.call(['list_keybinds']).stdout.strip() == ''


def test_keys_inactive_affects_release_binds(hlwm, keyboard):
    hlwm.call(['new_attr', 'string', 'my_test', 'initial'])
    hlwm.call(['keybind', 'Release-x', 'set_attr', 'my_test', 'release'])
    winid, proc = hlwm.create_client(term_command='read -n 1')
    hlwm.attr.clients[winid].keys_inactive = 'x'

    keyboard.press('x')

    # since 'x' is inactive, it should not have changed the attribute
    assert hlwm.attr.my_test() == 'initial'
    # ... and instead shut down the client:
    try:
        print(f"waiting for client proc {proc.pid}")
        proc.wait(PROCESS_SHUTDOWN_TIME)
    except subprocess.TimeoutExpired:
        assert False, "Expected client to quit, but it is still running"


def test_key_release_after_unbinding_press(hlwm, keyboard):
    hlwm.call(['new_attr', 'string', 'my_test', 'initial'])
    hlwm.call(['keybind', 'Mod1-x', 'set_attr', 'my_test', 'press'])
    hlwm.call(['keybind', 'Release-Mod1-x', 'set_attr', 'my_test', 'release'])
    hlwm.call(['keyunbind', 'Mod1-x'])
    assert hlwm.attr.my_test() == 'initial'

    keyboard.down('alt+x')
    assert hlwm.attr.my_test() == 'initial'  # because we had removed the binding

    keyboard.up('x')
    assert hlwm.attr.my_test() == 'release'


def test_key_release_reuses_mod_mask(hlwm, keyboard):
    """
    This verifies that in the key release event, we use the modifier mask
    that was once active back then when the key press happend.
    """
    hlwm.call(['new_attr', 'string', 'my_test', 'initial'])
    hlwm.call(['keybind', 'Mod1-x', 'set_attr', 'my_test', 'press'])
    hlwm.call(['keybind', 'Release-Mod1-x', 'set_attr', 'my_test', 'release'])
    assert hlwm.attr.my_test() == 'initial'

    keyboard.down('alt')
    keyboard.down('x')
    assert hlwm.attr.my_test() == 'press'

    keyboard.up('alt')
    keyboard.down('control')
    assert hlwm.attr.my_test() == 'press'

    # this triggers the 'release' binding:
    keyboard.up('x')
    assert hlwm.attr.my_test() == 'release'
    keyboard.up('control')
    assert hlwm.attr.my_test() == 'release'


def test_key_release_of_modifier(hlwm, keyboard):
    hlwm.call(['new_attr', 'string', 'my_test', 'initial'])
    hlwm.call(['keybind', 'Super_L', 'set_attr', 'my_test', 'press'])
    hlwm.call(['keybind', 'Release-Super_L', 'set_attr', 'my_test', 'release'])
    assert hlwm.attr.my_test() == 'initial'
    keyboard.down('Super_L')
    assert hlwm.attr.my_test() == 'press'
    keyboard.up('Super_L')
    assert hlwm.attr.my_test() == 'release'


def test_key_press_after_unbinding_release(hlwm, keyboard):
    hlwm.call(['new_attr', 'string', 'my_test', 'initial'])
    hlwm.call(['keybind', 'Mod1-x', 'set_attr', 'my_test', 'press'])
    hlwm.call(['keybind', 'Release-Mod1-x', 'set_attr', 'my_test', 'release'])
    hlwm.call(['keyunbind', 'Release-Mod1-x'])
    assert hlwm.attr.my_test() == 'initial'

    keyboard.down('alt+x')
    assert hlwm.attr.my_test() == 'press'  # because we had removed the binding

    # the modifier 'alt' still needs to be pressed during the following event:
    keyboard.up('x')
    assert hlwm.attr.my_test() == 'press'


@pytest.mark.parametrize('bind_press', [True, False])
@pytest.mark.parametrize('bind_release', [True, False])
@pytest.mark.parametrize('unbind_press', [True, False])
@pytest.mark.parametrize('unbind_release', [True, False])
@pytest.mark.parametrize('client_spawn_position', ['pre_bind', 'pre_unbind', 'last'])
def test_client_does_not_see_bound_key(hlwm, keyboard, bind_press, bind_release, unbind_press, unbind_release, client_spawn_position):
    hlwm.call(['new_attr', 'string', 'my_test', 'initial'])

    if client_spawn_position == 'pre_bind':
        winid, proc = hlwm.create_client(term_command='read -n 1')

    if bind_press:
        hlwm.call(['keybind', 'z', 'set_attr', 'my_test', 'grabbed'])
    if bind_release:
        hlwm.call(['keybind', 'Release+z', 'set_attr', 'my_test', 'grabbed'])

    if client_spawn_position == 'pre_unbind':
        winid, proc = hlwm.create_client(term_command='read -n 1')

    if unbind_press:
        # run unchecked, because the command will fail if it was not bound
        result = hlwm.unchecked_call(['keyunbind', 'z'])
        assert (result.returncode == 0) == bind_press
    if unbind_release:
        result = hlwm.unchecked_call(['keyunbind', 'Release-z'])
        assert (result.returncode == 0) == bind_release

    if client_spawn_position == 'last':
        winid, proc = hlwm.create_client(term_command='read -n 1')

    assert proc is not None
    assert winid is not None
    assert hlwm.attr.clients.focus.winid() == winid

    client_gets_event = (not bind_press or unbind_press) and (not bind_release or unbind_release)

    keyboard.press('z')

    if client_gets_event:
        assert hlwm.attr.my_test() == "initial"
        try:
            print(f"waiting for client proc {proc.pid}")
            proc.wait(PROCESS_SHUTDOWN_TIME)
        except subprocess.TimeoutExpired:
            assert False, "Expected client to quit, but it is still running"
    else:
        assert hlwm.attr.my_test() == "grabbed"
