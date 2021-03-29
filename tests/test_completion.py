import pytest
import re
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
    stack
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
            arg = shlex.split(arg)[0]  # strip trailing ' ' and evaluate escapes
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


@pytest.mark.exclude_from_coverage(
    reason='This test does not verify functionality but only whether the \
    commands can be called at all.')
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
        'lower': {3},
        'jumpto': {3},
        'remove_monitor': {6},
        'use_index': {3},
        'move_index': {3},
        'bring': {3},
        '!': {1},
        'shift': {6},
        'focus': {6},
    }
    allowed_stderr = re.compile('({})'.format('|'.join([
        'A (monitor|tag) with the name.*already exists',
        'No such.*client: urgent',  # for apply_rules
        'Could not find client "(urgent|)"',  # for drag
        'No neighbour found',  # for resize and similar commands
        r'There are no \(non-minimized\) floating windows; cannot focus',  # for floating_focused
    ])))
    # a set of commands that make other commands break
    # hence we need to run them separately
    destructive_commands = {
        'unsetenv',
        'split',  # the 'split' commands makes 'FrameLeaf' objects disappear
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
        result = hlwm.unchecked_call(command, log_output=False)
        assert result.returncode in returncodes \
            or allowed_stderr.search(result.stderr), \
            "Running " + ' '.join(command)


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


@pytest.mark.parametrize('command_prefix', [
    ['substitute', 'ARG', 'tags.count'],
    ['foreach', 'ARG', 'clients']
])
def test_metacommand(hlwm, command_prefix):
    """test the completion of a command that accepts another
    command as the parameter
    """
    cmdlist = hlwm.call('list_commands').stdout.splitlines()
    assert hlwm.complete(command_prefix) \
        == sorted(['ARG'] + cmdlist)


def test_posix_escape_via_use(hlwm):
    tags = [r'tag"with\special', 'a&b', '$dollar', '(paren)', 'foo~bar', '~foo']
    for t in tags:
        hlwm.call(['add', t])
    for command in ['move', 'use']:
        results = hlwm.complete(['use'], evaluate_escapes=True)
        assert sorted(['default'] + tags) == sorted(results)


def test_posix_escape_via_pad(hlwm):
    res = hlwm.complete(['pad', '0'])
    assert '\'\'' in res
    assert '0' in res


@pytest.mark.exclude_from_coverage(
    reason='This test does not verify functionality but only whether \
    passing junk to args does not crash herbstluftwm.')
@pytest.mark.parametrize("args_before", [0, 1, 2])
@pytest.mark.parametrize("junk_arg", ['junk', '234', ' '])
def test_junk_args_dont_crash(hlwm, args_before, junk_arg):
    commands = hlwm.call('list_commands').stdout.splitlines()
    for cmd_name in commands:
        if cmd_name in ['wmexec', 'quit']:
            continue
        full_cmd = [cmd_name]
        for _ in range(0, args_before):
            # find some arg appropriate for the current command
            completions = hlwm.unchecked_call(
                ['complete', str(len(full_cmd))] + full_cmd).stdout.splitlines()
            completions.append('')
            full_cmd.append(completions[0])
        full_cmd.append(junk_arg)
        hlwm.unchecked_call(full_cmd)


def test_optional_args_in_argparse_dont_pile_up(hlwm):
    hlwm.call(['move_monitor', '0', '400x300+0+0', '1', '1', '1', '1'])
    commands = [
        ['move_monitor', '0', '400x300+0+0'],
        ['move_monitor', '0', '400x300+0+0', '0'],
        ['move_monitor', '0', '400x300+0+0', '0', '0'],
        ['move_monitor', '0', '400x300+0+0', '0', '0', '0'],
    ]
    for cmd in commands:
        res = hlwm.complete(cmd, evaluate_escapes=True)
        assert len([v for v in res if v == '0']) == 1
