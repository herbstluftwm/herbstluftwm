import pytest
import shlex
from time import sleep

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


# def command2str(command):
#     s = ' '.join(command)
#     maxlen = 20
#     if len(s) > maxlen:
#         s = s[0:maxlen - 3] + '...'
#     return s


# TODO: can we use something like this?
# def pytest_generate_tests(metafunc):
#     if 'completable_command' in metafunc.fixturenames:
#         metafunc.parametrize('completable_command',
#         [
#             ['echo', 'foo'],
#             ['echo', 'foo', 'some very very very long argument'],
#             ['try', 'false'],
#             ['true'],
#         ], ids=command2str)

def generate_command_argument(hlwm, command, argument_prefix, steps):
    """
    generate the next argument of the command which has the given
    argument_prefix.

    This function makes at most 'step' many recursive calls.

    It returns a list of full arguments starting with the argument_prefix.
    """
    if steps <= 0:
        return []
    if command == []:
        return []
    ignore_commands = ['keybind', 'mousebind']
    if command[-1] in ignore_commands:
        return []
    hc_cmd = ['complete_shell', len(command)] + command + [argument_prefix]
    proc = hlwm.unchecked_call(hc_cmd, log_output=False)
    assert proc.returncode == 0, "completion failed for " + str(hc_cmd)
    completion_suggestions = set(proc.stdout.split('\n'))
    results = []
    for arg in completion_suggestions:
        if arg.endswith(' '):
            results.append(arg[0:-1])
        else:
            if len(arg) <= len(argument_prefix):
                continue
            results += generate_command_argument(hlwm, command, arg, steps - 1)
    return results


def generate_commands(hlwm, length, steps_per_argument=4, prefix=[]):
    """
    yield all commands of a given maximal length (plus the given prefix) that
    do not accept further arguments according to the completion.

    When generating parameters, allow at most `steps_per_argument` many
    completion steps per parameter.
    """
    if length < 0:
        return []
    proc = hlwm.unchecked_call(['complete_shell', len(prefix)] + prefix,
                               log_output=False)
    if proc.returncode == 7:
        # if no more parameter expected,
        # then prefix is a full command already:
        return [prefix]
    assert proc.returncode == 0, "completion failed for " + str(prefix)
    if prefix == []:
        generate_commands.commands_list = set(proc.stdout.split('\n'))
    commands = []
    # otherwise
    completion_suggestions = set(proc.stdout.split('\n'))
    if generate_commands.commands_list <= completion_suggestions \
            and len(generate_commands.commands_list) > 0 \
            and prefix != []:
        # if the commands are among the completions, then remove
        # all the commands except for 'true'
        completion_suggestions -= generate_commands.commands_list
        completion_suggestions.add('true ')
    for arg in completion_suggestions:
        if arg == '':
            # TODO: where does the empty string come from?
            continue
        if arg.endswith(' '):
            arg = arg[0:-1]  # strip trailing ' '
            if arg in prefix:
                # ignore commands where the same flag is passed twice
                continue
            commands += generate_commands(hlwm,
                                          length - 1,
                                          steps_per_argument,
                                          prefix + [arg])
        else:
            args = generate_command_argument(hlwm, prefix, arg,
                                             steps_per_argument)
            for a in args:
                commands += generate_commands(hlwm,
                                              length - 1,
                                              steps_per_argument,
                                              prefix + [a])
    return commands


generate_commands.commands_list = set([])


def test_generate_completable_commands(hlwm, request):
    # run pytest with --cache-clear to force renewal
    if request.config.cache.get('all_completable_commands', None) is None:
        cmds = generate_commands(hlwm, 4)
        request.config.cache.set('all_completable_commands', cmds)


@pytest.mark.parametrize('run_destructives', [False, True])
def test_completable_commands(hlwm, request, run_destructives):
    # wait for test_generate_completable_commands to finish
    # Note that for run_destructives=True, we need a fresh hlwm
    # instance.
    commands = None
    while commands is None:
        commands = request.config.cache.get('all_completable_commands', None)
        sleep(0.5)
    allowed_returncodes = {
        'false': {1},
        'close': {3},
        'raise': {3},
        'jumpto': {3},
        'remove_monitor': {6},
        'use_index': {3},
        'bring': {3},
        '!': {1},
        'shift': {6},
        'focus': {6},
    }
    # a set of commands that make other commands break
    # hence we need to run them separately
    destructive_commands = {
        'unsetenv'
    }
    for command in commands:
        if 'quit' in command:
            continue
        # the command mentions a destructive command,
        # if and only if the sets are not disjoint
        mentions_destructive_command = \
            not(destructive_commands.isdisjoint(set(command)))
        if mentions_destructive_command != run_destructives:
            # run commands involving destructive commands
            # iff run_destructives is set, and skip otherwise
            continue
        returncodes = {0}
        for k, v in allowed_returncodes.items():
            if k in command:
                returncodes |= v
        assert hlwm.unchecked_call(command, log_output=False).returncode \
            in returncodes, "Running " + ' '.join(command)


@pytest.mark.parametrize('name', commands_without_input)
def test_inputless_commands(hlwm, name):
    # FIXME: document exit code. Here, 7 = NO_PARAMETER_EXPECTED
    assert hlwm.call_xfail_no_output('complete 1 ' + name) \
        .returncode == 7


def test_remove_attr(hlwm):
    attr_path = "monitors.my_test"
    # assume you have a user-defined attribute
    hlwm.call("new_attr bool " + attr_path)
    hlwm.get_attr(attr_path)
    # then run all the available command completions
    cmds = generate_commands(hlwm,
                             1,
                             steps_per_argument=2,
                             prefix=['remove_attr'])
    for c in cmds:
        hlwm.call(c)
    # then expect that the attribute is gone
    hlwm.call_xfail('get_attr ' + attr_path)
