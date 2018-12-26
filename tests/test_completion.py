import pytest
import shlex

commands_without_input = shlex.split(
    """
        quit true false reload version
        list_commands list_monitors list_rules
        mouseunbind rotate
        lock unlock
    """)


@pytest.mark.parametrize('name', commands_without_input)
def test_inputless_commands(hlwm, name):
    assert hlwm.call_xfail_no_output('complete 1 ' + name) \
        .returncode == 7
