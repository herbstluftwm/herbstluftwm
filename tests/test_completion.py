import pytest
import shlex

commands_without_input = shlex.split(
    """
    close_and_remove
    close_or_remove
    false
    list_commands
    list_keybinds
    list_monitors
    list_rules
    lock
    mouseunbind
    quit
    reload
    remove
    rotate
    true
    unlock
    use_previous
    version
    """)


@pytest.mark.parametrize('name', commands_without_input)
def test_inputless_commands(hlwm, name):
    # FIXME: document exit code. Here, 7 = NO_PARAMETER_EXPECTED
    assert hlwm.call_xfail_no_output('complete 1 ' + name) \
        .returncode == 7
