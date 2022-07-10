import pytest
from Xlib import Xutil
from herbstluftwm.types import Rectangle


string_props = [
    'instance',
    'class',
    'title',
    'windowtype',
    'windowrole',
]

numeric_props = [
    'pid',
    'pgid',
    'maxage',
]

consequences = [
    'tag',
    'monitor',
    'focus',
    'switchtag',
    'manage',
    'index',
    'floating',
    'floating_geometry',
    'pseudotile',
    'ewmhrequests',
    'ewmhnotify',
    'fullscreen',
    'hook',
    'keymask',
    'keys_inactive',
    'floatplacement',
]


def test_list_rules_empty_by_default(hlwm):
    rules = hlwm.call('list_rules')

    assert rules.stdout == ''


def test_add_simple_rule(hlwm):
    hlwm.call('rule class=Foo tag=bar')

    rules = hlwm.call('list_rules')
    assert rules.stdout == 'label=0\tclass=Foo\ttag=bar\t\n'


def test_add_simple_rule_with_dashes(hlwm):
    hlwm.call('rule --class=Foo --tag=bar')

    rules = hlwm.call('list_rules')
    assert rules.stdout == 'label=0\tclass=Foo\ttag=bar\t\n'


def test_add_many_labeled_rules(hlwm):
    # Add set of rules with every valid combination of
    # property and match operator appearing at least once:

    # Make a single, long list of all consequences which accept any string:
    consequences_str = \
        ' '.join(['{}=a{}b'.format(c, idx)
                  for idx, c in enumerate(['tag', 'monitor'], start=4117)])

    # Make three sets of long conditions lists: for numeric matches, string
    # equality and regexp equality:
    conds_sets = [
        ' '.join(['{}={}'.format(prop, idx) for idx, prop in enumerate(numeric_props, start=9001)]),
        ' '.join(['{}=x{}y'.format(prop, idx) for idx, prop in enumerate(string_props, start=9101)]),
        ' '.join(['{}~z{}z'.format(prop, idx) for idx, prop in enumerate(string_props, start=9201)]),
    ]

    # Assemble final list of rules:
    rules = []
    for idx, conds in enumerate(conds_sets):
        rules.append('label=l{} {} {}'.format(idx, conds, consequences_str))

    for rule in rules:
        hlwm.call('rule ' + rule)
    list_rules = hlwm.call('list_rules')

    expected_stdout = ''.join([rule.replace(' ', '\t') + '\t\n' for rule in rules])
    assert list_rules.stdout == expected_stdout


@pytest.mark.parametrize('command', ['rule', 'apply_tmp_rule --all'])
def test_add_rule_with_misformatted_argument(hlwm, command):
    call = hlwm.call_xfail(f'{command} notevenanoperator')

    arg0 = command.split(' ')[0]  # strip arguments
    call.expect_stderr(f'{arg0}: No operator in given arg: notevenanoperator')


@pytest.mark.parametrize('command', ['rule', 'apply_tmp_rule --all'])
def test_cannot_add_rule_with_empty_label(hlwm, command):
    call = hlwm.call_xfail(f'{command} label= class=Foo tag=bar')

    commandname = command.split(' ')[0]
    assert call.stderr == f'{commandname}: Rule label cannot be empty\n'


@pytest.mark.parametrize('command', ['rule', 'apply_tmp_rule --all'])
def test_cannot_use_tilde_operator_for_rule_label(hlwm, command):
    call = hlwm.call_xfail(f'{command} label~bla class=Foo tag=bar')

    commandname = command.split(' ')[0]
    assert call.stderr == f'{commandname}: Unknown rule label operation "~"\n'


@pytest.mark.parametrize('command', ['rule', 'apply_tmp_rule --all'])
def test_add_rule_with_unknown_condition(hlwm, command):
    call = hlwm.call_xfail(f'{command} foo=bar quit')
    arg0 = command.split(' ')[0]  # strip arguments
    call.expect_stderr(f'{arg0}: Unknown argument "foo=bar"')


@pytest.mark.parametrize('command', ['rule', 'apply_tmp_rule --all'])
def test_add_rule_maxage_condition_operator(hlwm, command):
    call = hlwm.call_xfail(f'{command} maxage~12')
    call.expect_stderr('rule: Condition maxage only supports the = operator')


@pytest.mark.parametrize('command', ['rule', 'apply_tmp_rule --all'])
def test_add_rule_maxage_condition_integer(hlwm, command):
    call = hlwm.call_xfail(f'{command} maxage=foo')
    call.expect_stderr('rule: Cannot parse integer from "foo"')


@pytest.mark.parametrize('method', ['-F', '--all'])
def test_remove_all_rules(hlwm, method):
    hlwm.call('rule class=Foo tag=bar')
    hlwm.call('rule label=labeled class=Bork tag=baz')

    hlwm.call(['unrule', method])

    rules = hlwm.call('list_rules')
    assert rules.stdout == ''


def test_remove_simple_rule(hlwm):
    hlwm.call('rule class=Foo tag=bar')

    hlwm.call('unrule 0')

    rules = hlwm.call('list_rules')
    assert rules.stdout == ''


def test_remove_labeled_rule(hlwm):
    hlwm.call('rule label=blah class=Foo tag=bar')

    hlwm.call('unrule blah')

    rules = hlwm.call('list_rules')
    assert rules.stdout == ''


def test_remove_nonexistent_rule(hlwm):
    hlwm.call_xfail('unrule nope') \
        .expect_stderr('Couldn\'t find any rules with label "nope"')


def test_singleuse_rule_disappears_after_matching(hlwm):
    hlwm.call('rule once hook=dummy_hook')

    hlwm.create_client()

    assert hlwm.call('list_rules').stdout == ''


@pytest.mark.parametrize('rules_count', [1, 2, 10])
def test_rule_labels_are_not_reused(hlwm, rules_count):
    # First add some rules and remove them again
    for i in range(rules_count):
        hlwm.call('rule class=Foo{0} tag=bar{0}'.format(i))
    for i in range(rules_count):
        hlwm.call('unrule {}'.format(i))

    # Add back a single new rule
    hlwm.call('rule class=meh tag=moo')

    # Remaining rule has a high label number (not zero)
    rules = hlwm.call('list_rules')
    assert rules.stdout == 'label={}\tclass=meh\ttag=moo\t\n'.format(rules_count)


def test_cannot_use_invalid_operator_for_consequence(hlwm):
    call = hlwm.call_xfail('rule class=Foo tag~bar')

    assert call.stderr == 'rule: Operator ~ not valid for consequence "tag"\n'


@pytest.mark.parametrize('rules_count', [1, 2, 10])
def test_complete_unrule_offers_all_rules(hlwm, rules_count):
    rules = [str(i) for i in range(rules_count)]
    for i in rules:
        hlwm.call('rule class=Foo{0} tag=bar{0}'.format(i))

    assert hlwm.complete('unrule') == sorted(rules + ['-F', '--all'])


def test_complete_rule(hlwm):
    assert hlwm.complete('rule', partial=True) == sorted(
        [i + ' ' for i in '! not prepend fixedsize once printlabel'.split(' ')]
        + [i + '=' for i in string_props + numeric_props]
        + [i + '~' for i in string_props + numeric_props]
        + [i + '=' for i in consequences + ['label']]
    )


@pytest.mark.parametrize('monitor_spec', ['monitor2', '1'])
def test_monitor_consequence(hlwm, monitor_spec):
    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+40+40 tag2 monitor2')
    assert hlwm.get_attr('monitors.focus.name') == ''

    hlwm.call('rule monitor=' + monitor_spec)
    winid, _ = hlwm.create_client()

    assert hlwm.get_attr('clients', winid, 'tag') == 'tag2'


def test_invalid_regex_in_condition(hlwm):
    call = hlwm.call_xfail('rule class~[b-a]')

    assert call.stderr == 'rule: Cannot parse value "[b-a]" from condition "class": "Invalid range in bracket expression."\n'


def test_printlabel_flag(hlwm):
    call1 = hlwm.call('rule printlabel label=bla class=Foo')
    call2 = hlwm.call('rule printlabel class=Foo')

    assert call1.stdout == 'bla\n'
    assert call2.stdout == '1\n'


def test_prepend_flag(hlwm):
    hlwm.call('rule class=AddedFirst')
    hlwm.call('rule prepend class=AddedSecond')
    rules = hlwm.call('list_rules')

    assert rules.stdout == \
        'label=1\tclass=AddedSecond\t\n' + \
        'label=0\tclass=AddedFirst\t\n'


def test_not_flag(hlwm):
    hlwm.call('rule not class=someclass')
    rules = hlwm.call('list_rules')

    assert rules.stdout == 'label=0\tnot\tclass=someclass\t\n'


@pytest.mark.parametrize('negation', ['not', '!'])
def test_condition_must_come_after_negation(hlwm, negation):
    call = hlwm.call_xfail(['rule', negation])

    assert call.stderr == f'rule: Expected another argument after "{negation}" flag\n'


def test_condition_string_match(hlwm):
    hlwm.call('add tag2')

    hlwm.call('rule title=foo tag=tag2')
    winid, _ = hlwm.create_client(title='foo')

    assert hlwm.get_attr('clients', winid, 'tag') == 'tag2'


def test_condition_regexp_match(hlwm):
    hlwm.call('add tag2')

    hlwm.call('rule title~ba+r tag=tag2')
    winid, _ = hlwm.create_client(title='baaaaar')

    assert hlwm.get_attr('clients', winid, 'tag') == 'tag2'


def test_condition_maxage(hlwm):
    hlwm.call('add tag2')

    hlwm.call('rule maxage=1 tag=tag2')
    import time
    time.sleep(2)
    winid, _ = hlwm.create_client()

    assert hlwm.get_attr('clients', winid, 'tag') == 'default'


def test_condition_numeric_equal(hlwm):
    hlwm.call('add tag2')

    hlwm.call('rule not pid=1 tag=tag2')
    winid, _ = hlwm.create_client()

    assert hlwm.get_attr('clients', winid, 'tag') == 'tag2'


def test_condition_instance(hlwm):
    hlwm.call('add tag2')

    # Note: We're relying on the knowledge that xterm is used as test client:
    hlwm.call('rule instance=xterm tag=tag2')
    winid, _ = hlwm.create_client()

    assert hlwm.get_attr('clients', winid, 'tag') == 'tag2'


def test_condition_class(hlwm):
    hlwm.call('add tag2')

    # Note: We're relying on knowledge about the test client class here:
    hlwm.call('rule class~client_.* tag=tag2')
    winid, _ = hlwm.create_client()

    assert hlwm.get_attr('clients', winid, 'tag') == 'tag2'


@pytest.mark.parametrize('rulearg,errormsg', [
    ("fullscreen=foo", 'only.*are valid booleans'),
    ("keymask=(", r'(Parenthesis is not closed|Mismatched.*\(.*\).*in regular)'),
    ("floatplacement=bar", 'Expecting one of: center, '),
    ("floating_geometry=4x5+1024+1024", 'Rectangle too small'),
    ("floating_geometry=totallywrong", 'Rectangle too small'),  # TODO: make rectangle parsing fail
])
@pytest.mark.parametrize('command', ['rule', 'apply_tmp_rule'])
def test_consequence_parse_error_correct_error_message(hlwm, command, rulearg, errormsg):
    full_cmd = [command, rulearg]
    if command == 'apply_tmp_rule':
        winid, _ = hlwm.create_client()
        full_cmd = [command, winid, rulearg]

    hlwm.call_xfail(full_cmd) \
        .expect_stderr(errormsg)


def test_consequence_parse_error_general(hlwm):
    """
    Test that the rule parsing can fail for almost all consequences
    and roughly check the error message
    """
    for c in consequences:
        if c in ['tag', 'monitor', 'index', 'hook']:
            continue
        invalid_arg = 'invalid(arg'  # invalid for any type!
        invalid_arg_escaped = r'invalid\(arg'
        hlwm.call_xfail(['rule', f'{c}={invalid_arg}']) \
            .expect_stderr(f'Invalid.*"{invalid_arg_escaped}".*to.*"{c}":')


class RuleMode:
    """How to create a client with a rule"""
    RULE_FIRST = 0  # first add a rule, then create a client
    APPLY_RULES = 1  # first the client, then the rule , then 'apply_rules'
    APPLY_TMP_RULE = 2  # first the client, then 'apply_tmp_rule' with the rule
    values = [RULE_FIRST, APPLY_RULES, APPLY_TMP_RULE]


def create_client(hlwm, rule_mode: RuleMode, rule):
    """create a client and the given rule. if client_before_rules, then
    first create the client, and later apply the rule to it via the
    apply_rules command"""
    if rule_mode == RuleMode.APPLY_RULES:
        winid, _ = hlwm.create_client()
        hlwm.call(['rule'] + rule)
        hlwm.call(['apply_rules', winid])
    elif rule_mode == RuleMode.RULE_FIRST:
        hlwm.call(['rule'] + rule)
        winid, _ = hlwm.create_client()
    else:
        assert rule_mode == RuleMode.APPLY_TMP_RULE
        winid, _ = hlwm.create_client()
        hlwm.call(['apply_tmp_rule', winid] + rule)
    return winid


@pytest.mark.parametrize(
    'name',
    ['floating', 'pseudotile', 'fullscreen', 'ewmhrequests', 'ewmhnotify', 'fullscreen'])
@pytest.mark.parametrize('value', [True, False])
@pytest.mark.parametrize('rule_mode', RuleMode.values)
def test_bool_consequence_with_corresponding_attribute(hlwm, name, value, rule_mode):
    winid = create_client(hlwm, rule_mode, [name + '=' + hlwm.bool(value)])

    assert hlwm.get_attr('clients.{}.{}'.format(winid, name)) == hlwm.bool(value)


@pytest.mark.parametrize(
    'name',
    ['keymask', 'keys_inactive'])
@pytest.mark.parametrize('rule_mode', RuleMode.values)
def test_regex_consequence_with_corresponding_attribute(hlwm, name, rule_mode):
    value = 'someregex'
    winid = create_client(hlwm, rule_mode, [name + '=' + value])

    assert hlwm.get_attr('clients.{}.{}'.format(winid, name)) == value


@pytest.mark.parametrize('manage', ['on', 'off'])
def test_unmanage(hlwm, x11, manage):
    hlwm.call(['rule', 'once', 'manage=' + manage])

    _, winid = x11.create_client()

    expected_children = [] if manage == 'off' else [winid, 'focus']
    assert hlwm.list_children('clients') == expected_children


@pytest.mark.parametrize('force_unmanage', [False, True])
def test_rule_hook_unmanaged(hlwm, x11, hc_idle, force_unmanage):
    # here, we test that a new window fires a rule with a hook, regardless
    # of whether the window is managable or not (e.g. menus). The crucial case
    # is of course for force_unmanage=True.
    hlwm.call('rule hook=somewindow')

    _, winid = x11.create_client(force_unmanage=force_unmanage)

    assert ['rule', 'somewindow', str(winid)] in hc_idle.hooks()


def test_apply_rules_nothing_changes(hlwm, x11):
    winid, _ = hlwm.create_client()

    hlwm.call(['apply_rules', winid])


@pytest.mark.parametrize('apply_rules', [True, False])
def test_move_tag(hlwm, apply_rules):
    hlwm.call('add tag2')
    winid = create_client(hlwm, apply_rules, ['tag=tag2'])

    assert hlwm.get_attr('clients.{}.tag'.format(winid)) == 'tag2'


@pytest.mark.parametrize('tag_on_mon', ['tag2', 'tag3'])
@pytest.mark.parametrize('apply_rules', [True, False])
def test_move_tag_with_monitor(hlwm, apply_rules, tag_on_mon):
    hlwm.call('add tag2')
    hlwm.call('add tag3')
    hlwm.call('add_monitor 600x600+600+0 {} mon2'.format(tag_on_mon))
    winid = create_client(hlwm, apply_rules, ['tag=tag2'])

    assert hlwm.get_attr('clients.{}.tag'.format(winid)) == 'tag2'


@pytest.mark.parametrize('focus', [True, False])
@pytest.mark.parametrize('apply_rules', [True, False])
def test_focus(hlwm, focus, apply_rules):
    other, _ = hlwm.create_client()
    winid = create_client(hlwm, apply_rules, ['focus=' + hlwm.bool(focus)])

    # check that the second client is focused iff (if and only if) the
    # rule said so
    assert (hlwm.get_attr('clients.focus.winid') == winid) == focus


@pytest.mark.parametrize('from_other_mon', [True, False])
def test_bring(hlwm, from_other_mon):
    # place a client on another tag
    hlwm.call('add tag2')
    # thats possibly displayed on another monitor
    if from_other_mon:
        hlwm.call('add_monitor 600x600+600+0 tag2')
    hlwm.call('rule once tag=tag2')
    winid, _ = hlwm.create_client()
    assert hlwm.get_attr('clients.{}.tag'.format(winid)) == 'tag2'

    # apply rules that bring the client here
    hlwm.call('sprintf T tag=%s tags.focus.name rule once T')
    hlwm.call(['apply_rules', winid])

    assert hlwm.get_attr('clients.{}.tag'.format(winid)) \
        == hlwm.get_attr('tags.focus.name')


def test_wm_class_too_late(hlwm, x11):
    hlwm.call('add tag2')
    hlwm.call('rule class=SomeTmpClassTooLate tag=tag2')
    winref, winid = x11.create_client()
    assert hlwm.get_attr('clients.{}.tag'.format(winid)) != 'tag2'

    # change window class after the window has appeared already
    winref.set_wm_class('someInstance', 'SomeTmpClassTooLate')
    x11.display.sync()

    assert hlwm.get_attr('clients.{}.tag'.format(winid)) == 'tag2'


def test_apply_rules_all_no_focus(hlwm):
    hlwm.call('add tag1')
    hlwm.call('rule title=c1 tag=tag1')
    client1, _ = hlwm.create_client(title='c1')
    hlwm.call('add tag2')
    hlwm.call('rule title=c2 tag=tag2')
    client2, _ = hlwm.create_client(title='c2')
    assert hlwm.get_attr('clients.{}.tag'.format(client1)) == 'tag1'
    assert hlwm.get_attr('clients.{}.tag'.format(client2)) == 'tag2'
    assert 'focus' not in hlwm.list_children('clients')

    # change rules and apply them
    hlwm.call('unrule -F')
    hlwm.call('rule title=c1 tag=tag2')
    hlwm.call('rule title=c2 tag=tag1')
    hlwm.call('apply_rules --all')

    assert hlwm.get_attr('clients.{}.tag'.format(client1)) == 'tag2'
    assert hlwm.get_attr('clients.{}.tag'.format(client2)) == 'tag1'
    assert 'focus' not in hlwm.list_children('clients')


@pytest.mark.parametrize('mode', ['apply_rules', 'apply_tmp_rule'])
def test_apply_rules_all_focus_retained(hlwm, mode):
    # test that passing --all to apply_rules or apply_tmp_rule
    # does not change the focus.
    if mode == 'apply_rules':
        hlwm.call('rule focus=on pseudotile=on')
    client1, _ = hlwm.create_client()
    client2, _ = hlwm.create_client()
    client3, _ = hlwm.create_client()
    # check that the focus is retained, no matter in which
    # order they appear in the hash_map:
    all_clients = [client1, client2, client3]
    for client in all_clients:
        hlwm.call(['jumpto', client])
        assert hlwm.get_attr('clients.focus.winid') == client
        for c in all_clients:
            # reset 'pseudotile' to off to verify that
            # the rules were indeed evaluated and only 'focus' got ignored
            hlwm.call(f'set_attr clients.{c}.pseudotile off')

        if mode == 'apply_rules':
            hlwm.call('apply_rules --all')
        else:
            hlwm.call('apply_tmp_rule --all focus=on pseudotile=on')

        # the rules were evaluated
        for c in all_clients:
            assert hlwm.get_attr(f'clients.{c}.pseudotile') == hlwm.bool(True)
        # but the focus is where it was before
        assert hlwm.get_attr('clients.focus.winid') == client


@pytest.mark.parametrize('floating', [True, False])
@pytest.mark.parametrize('source_tag', range(0, 4))
@pytest.mark.parametrize('target_tag', range(0, 4))
def test_apply_rules_minimized_client(hlwm, floating, source_tag, target_tag):
    # set up some tags for different cases of focus/visibility:
    tags = [
        'on_focused_mon',
        'on_unfocused_mon',
        'unused_tag',
        'other_unused_tag',
    ]
    for t in tags:
        hlwm.call(['add', t])
    hlwm.call('use on_focused_mon')
    hlwm.call('add_monitor 800x600+300+300 on_unfocused_mon')
    # put a client on the source_tag
    hlwm.call(f'rule tag={tags[source_tag]} floating={hlwm.bool(floating)}')
    winid, _ = hlwm.create_client()
    # and minimize it there, so it is invisible in any case
    hlwm.call(f'set_attr clients.{winid}.minimized on')
    assert hlwm.get_attr(f'clients.{winid}.tag') == tags[source_tag]
    assert hlwm.get_attr(f'clients.{winid}.visible') == hlwm.bool(False)
    assert hlwm.get_attr(f'clients.{winid}.minimized') == hlwm.bool(True)
    assert hlwm.get_attr(f'clients.{winid}.floating') == hlwm.bool(floating)

    # move the minimized client from source_tag to target_tag
    hlwm.call(f'rule tag={tags[target_tag]}')
    hlwm.call(f'apply_rules {winid}')

    # check that the client is on the target_tag and still invisible
    assert hlwm.get_attr(f'clients.{winid}.tag') == tags[target_tag]
    assert hlwm.get_attr(f'clients.{winid}.visible') == hlwm.bool(False)
    assert hlwm.get_attr(f'clients.{winid}.minimized') == hlwm.bool(True)
    assert hlwm.get_attr(f'clients.{winid}.floating') == hlwm.bool(floating)


def test_rule_tag_nonexisting(hlwm):
    hlwm.call('rule tag=tagdoesnotexist')
    # must not crash:
    hlwm.create_client()


def test_no_rules_for_own_windows(hlwm):
    hlwm.call('add othertag')
    hlwm.create_client()
    hlwm.call('rule once pseudotile=on')
    hlwm.call('rule once focus=on')
    assert len(hlwm.call('list_rules').stdout.splitlines()) == 2

    # this must not fire the rules, even though the decoration
    # windows appear and disappear
    hlwm.call('use othertag')
    hlwm.call('split v')
    hlwm.call('use_index 0')

    assert len(hlwm.call('list_rules').stdout.splitlines()) == 2


@pytest.mark.parametrize('window_type', [
    '_NET_WM_WINDOW_TYPE_DESKTOP',
    '_NET_WM_WINDOW_TYPE_DOCK',
])
def test_desktop_window_not_managed(hlwm, hc_idle, x11, window_type):
    hlwm.call('rule hook=mywindow')
    _, winid = x11.create_client(sync_hlwm=False,
                                 window_type=window_type)
    assert ['rule', 'mywindow', winid] in hc_idle.hooks()
    assert hlwm.list_children('clients') == []


@pytest.mark.parametrize('window_type', [
    '_NET_WM_WINDOW_TYPE_DIALOG',
    '_NET_WM_WINDOW_TYPE_SPLASH',
])
def test_dialog_window_floated(hlwm, hc_idle, x11, window_type):
    hlwm.call('rule hook=mywindow')
    _, winid = x11.create_client(sync_hlwm=False,
                                 window_type=window_type)
    assert hlwm.get_attr(f'clients.{winid}.floating') == hlwm.bool(True)
    assert ['rule', 'mywindow', winid] in hc_idle.hooks()


@pytest.mark.parametrize('transient_for', [True, False])
def test_float_transient_for(hlwm, x11, transient_for):
    mainwin, mainwinid = x11.create_client()
    trans_for_win = None
    if transient_for:
        trans_for_win = mainwin
    _, winid = x11.create_client(transient_for=trans_for_win)

    assert hlwm.get_attr(f'clients.{mainwinid}.floating') == 'false'
    assert hlwm.get_attr(f'clients.{winid}.floating') == hlwm.bool(transient_for)


@pytest.mark.parametrize('command', ['apply_rules', 'apply_tmp_rule'])
def test_apply_rules_invalid_window(hlwm, command):
    hlwm.call_xfail([command, 'invalidWindowId']) \
        .expect_stderr(r'No such.*client')


def test_apply_rules_on_unmanaged_window(hlwm, hc_idle, x11):
    hlwm.call('rule manage=off hook=processed')
    _, winid = x11.create_client()
    # assert that hlwm correctly processed the window
    assert ['rule', 'processed', winid] in hc_idle.hooks()
    assert winid not in hlwm.list_children('clients')

    hlwm.call_xfail(['apply_rules', winid]) \
        .expect_stderr(r'No such.*client')


def test_apply_rules_unmanage(hlwm):
    winid, _ = hlwm.create_client()
    hlwm.call('rule manage=off')

    hlwm.call_xfail(['apply_rules', winid]) \
        .expect_stderr(r'not yet possible')


@pytest.mark.parametrize('focus', [True, False])
def test_switchtag(hlwm, focus):
    hlwm.call('add tag2')
    hlwm.call(['rule', 'title=switchme', 'tag=tag2', 'switchtag=true', 'focus=' + hlwm.bool(focus)])

    assert hlwm.get_attr('tags.focus.name') == 'default'
    winid, _ = hlwm.create_client(title='switchme')

    assert hlwm.get_attr('clients', winid, 'tag') == 'tag2'

    if focus:
        assert hlwm.get_attr('tags.focus.name') == 'tag2'
    else:
        assert hlwm.get_attr('tags.focus.name') == 'default'


@pytest.mark.parametrize('swap_monitors_to_get_tag', [True, False])
def test_switchtag_monitor(hlwm, swap_monitors_to_get_tag):
    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+800+0 tag2')
    hlwm.call(['set', 'swap_monitors_to_get_tag', hlwm.bool(swap_monitors_to_get_tag)])
    hlwm.call(['rule', 'title=switchme', 'tag=tag2', 'switchtag=true', 'focus=true'])

    assert hlwm.get_attr('tags.focus.name') == 'default'
    assert int(hlwm.get_attr('monitors.focus.index')) == 0
    winid, _ = hlwm.create_client(title='switchme')

    assert hlwm.get_attr('clients', winid, 'tag') == 'tag2'
    assert hlwm.get_attr('tags.focus.name') == 'tag2'

    if swap_monitors_to_get_tag:
        assert int(hlwm.get_attr('monitors.focus.index')) == 0
    else:
        assert int(hlwm.get_attr('monitors.focus.index')) == 1


@pytest.mark.parametrize('floatplacement', ['none', 'center'])
def test_floatplacement_none_or_center(hlwm, floatplacement, x11):
    # create sizes involving only even numbers
    hlwm.call('move_monitor 0 500x550+0+0')
    hlwm.call('floating on')
    hlwm.call('rule floatplacement={}'.format(floatplacement))
    winhandle, _ = x11.create_client(geometry=(30, 40, 600, 400))

    geom = x11.get_absolute_geometry(winhandle)
    # in any case, the size is not affected
    assert (geom.width, geom.height) == (600, 400)

    if floatplacement == 'center':
        window_center = (geom.x + 600 / 2, geom.y + 400 / 2)
        monitor_center = (500 / 2, 550 / 2)
        assert window_center == monitor_center
    else:
        assert (geom.x, geom.y) == (30, 40)


def test_floatplacement_uses_other_visible_monitor(hlwm, x11):
    hlwm.call('add othertag')
    hlwm.call('set_monitors 300x350+0+0 500x550+300+0')
    hlwm.call('focus_monitor 0')
    # move to second monitor
    hlwm.call('rule floatplacement=center tag=othertag')

    # even though monitor 0 is focused ...
    assert hlwm.call('monitor_rect').stdout == '0 0 300 350'
    # .. the window is centered on monitor 1:
    winhandle, _ = x11.create_client(geometry=(30, 40, 200, 250))
    geom = x11.get_absolute_geometry(winhandle)
    assert (geom.x + geom.width / 2, geom.y + geom.height / 2) \
        == (300 + 500 / 2, 550 / 2)


def test_floatplacement_for_invisible_tag(hlwm, x11):
    hlwm.call('move_monitor 0 500x550+0+0')
    hlwm.call('add othertag')
    hlwm.call('rule floatplacement=center tag=othertag')

    winhandle, _ = x11.create_client(geometry=(30, 40, 200, 250))
    # the window is not yet rendered because the tag is invisible
    # but as soon as we show it, the window is placed correctly
    hlwm.call('use othertag')
    geom = x11.get_absolute_geometry(winhandle)
    assert (geom.x + geom.width / 2, geom.y + geom.height / 2) \
        == (500 / 2, 550 / 2)


def assert_that_rectangles_do_not_overlap(rectangles):
    """given a list of rectangles and a description (any string)
    verify that all the rectangles are disjoint. The rectangles
    list consists of tuples, containing a rectangle and a description"""
    def geometry_to_string(rectangle):
        return '%dx%d%+d%+d' % (rectangle.width, rectangle.height,
                                rectangle.x, rectangle.y)
    for i, (rect1, description1) in enumerate(rectangles):
        for (rect2, description2) in rectangles[0:i]:
            # compute the intersection of rect1 and rect2.
            # code identical to Rectangle::intersectionWith()
            tl_x = max(rect1.x, rect2.x)
            tl_y = max(rect1.y, rect2.y)
            br_x = min(rect1.x + rect1.width, rect2.x + rect2.width)
            br_y = min(rect1.y + rect1.height, rect2.y + rect2.height)
            overlap = tl_x < br_x and tl_y < br_y
            assert not overlap, \
                '{} ({}) and {} ({}) overlap' \
                .format(description1, geometry_to_string(rect1),
                        description2, geometry_to_string(rect2))


@pytest.mark.parametrize('num_tiling_clients', [0, 2])
def test_floatplacement_smart_enough_space_for_all(hlwm, x11, num_tiling_clients):
    hlwm.call('move_monitor "" 500x800')
    hlwm.call('rule floatplacement=smart')

    # create a side-by-side split where potential tiling
    # clients are put into the left frame
    hlwm.call('split horizontal')

    clients = []
    for i in range(0, 5):
        if i >= num_tiling_clients:
            hlwm.call('rule floating=on')
        clients.append(x11.create_client(geometry=(30, 40, 200, 250)))

    rectangles = [(x11.get_absolute_geometry(handle), winid)
                  for handle, winid in clients]
    assert_that_rectangles_do_not_overlap(rectangles)


def test_floatplacement_smart_overlap_with_only_tiling(hlwm, x11):
    hlwm.call('move_monitor "" 500x800')
    hlwm.call('rule floatplacement=smart')

    # a side-by-side split
    hlwm.call('split horizontal')
    # the left frame occupied:
    x11.create_client(geometry=(30, 40, 800, 800))
    hlwm.call('rule floating=on')  # all other clients will be floating
    # the right frame is 400 px wide and we create two floating clients
    # where each of them is 201 px wide (plus border width, etc)

    clients = [
        x11.create_client(geometry=(30, 40, 200, 750)),
        x11.create_client(geometry=(30, 40, 200, 750)),
    ]
    # now the floating clients must be placed such that they do not overlap
    rectangles = [(x11.get_absolute_geometry(handle), winid)
                  for handle, winid in clients]
    assert_that_rectangles_do_not_overlap(rectangles)


def test_floatplacement_smart_uses_all_corners(hlwm, x11):
    hlwm.call('move_monitor "" 500x520')
    hlwm.call('rule floatplacement=smart floating=on')
    snap_gap = 5
    hlwm.call(f'set snap_gap {snap_gap}')
    hlwm.call('set window_border_width 0')

    # in this 500x500 area, place 4 clients 300x300 each
    clients = [x11.create_client(geometry=(30, 40, 300, 302))
               for _ in range(0, 4)]

    # in order to use the space efficiently, they must be placed
    # in the corners and overlap in the center.
    corner2client = {}  # a dict, mapping corners to the client
    for winhandle, winid in clients:
        geo = x11.get_absolute_geometry(winhandle)
        assert geo.width == 300
        assert geo.height == 302
        # check which of the screen edges is touched by the window
        touches_left = geo.x == snap_gap
        touches_right = geo.x + geo.width == 500 - snap_gap
        touches_top = geo.y == snap_gap
        touches_bottom = geo.y + geo.height == 520 - snap_gap
        # the combination of the above flags yields the corner
        corner = (touches_left, touches_right, touches_top, touches_bottom)
        assert corner2client.get(corner, None) is None, \
            f'Error: {winid} is not the only one in corner {corner}'
        corner2client[corner] = winid

    # there are 4 clients for 4 corners
    assert len(corner2client) == 4


@pytest.mark.exclude_from_coverage(
    reason='This test does not verify functionality but only whether \
    creating lots of windows can be handled by the algorithm')
@pytest.mark.parametrize('invisible_tag', [False, True])
def test_floatplacement_smart_create_many(hlwm, x11, invisible_tag):
    hlwm.call('move_monitor "" 500x520')
    if invisible_tag:
        hlwm.call('add invisible_tag')
        hlwm.call('rule tag=invisible_tag')
    hlwm.call('rule floatplacement=smart floating=on')

    # create many clients with different sizes
    def index2geometry(i):
        # vary between 2 different widths and 3 different
        # heights to get a lot of combinations
        return (30, 40, (i % 2) * 110, (i % 3) * 120)

    for i in range(0, 50):
        x11.create_client(geometry=index2geometry(i))


def test_floatplacement_smart_invisible_windows(hlwm):
    hlwm.call('add invisible')
    hlwm.call('rule floatplacement=smart floating=on tag=invisible')

    # create some floating clients on the invisible tag and check
    # that it does not crash
    hlwm.create_client()
    hlwm.create_client()
    hlwm.create_client()
    hlwm.create_client()


def test_apply_tmp_rule_ignores_other_clients_or_rules(hlwm):
    target, _ = hlwm.create_client()
    other, _ = hlwm.create_client()
    hlwm.call('rule pseudotile=on')
    for winid in [target, other]:
        hlwm.call(f'set_attr clients.{winid}.fullscreen off')
        hlwm.call(f'set_attr clients.{winid}.pseudotile off')

    hlwm.call(f'apply_tmp_rule {target} fullscreen=on')

    # only the target's fullscreen is set to true
    assert hlwm.get_attr(f'clients.{target}.fullscreen') == hlwm.bool(True)
    # everything else stays false
    assert hlwm.get_attr(f'clients.{target}.pseudotile') == hlwm.bool(False)
    assert hlwm.get_attr(f'clients.{other}.fullscreen') == hlwm.bool(False)
    assert hlwm.get_attr(f'clients.{other}.pseudotile') == hlwm.bool(False)
    # double check that the rule really would have worked:
    hlwm.call(f'apply_rules {target}')
    hlwm.get_attr(f'clients.{target}.pseudotile') == hlwm.bool(True)


def test_apply_tmp_rule_all_ignores_other_rules(hlwm):
    win1, _ = hlwm.create_client()
    win2, _ = hlwm.create_client()
    hlwm.call('rule pseudotile=on')
    for winid in [win1, win2]:
        hlwm.call(f'set_attr clients.{winid}.fullscreen off')
        hlwm.call(f'set_attr clients.{winid}.pseudotile off')

    hlwm.call('apply_tmp_rule --all fullscreen=on')

    # the temporary rule only sets the fullscreen, but 'pseudotile' is unchanged
    for winid in [win1, win2]:
        hlwm.get_attr(f'clients.{winid}.fullscreen') == hlwm.bool(True)
        hlwm.get_attr(f'clients.{winid}.pseudotile') == hlwm.bool(False)
    # double check that the rule really would have worked:
    hlwm.call('apply_rules --all')
    for winid in [win1, win2]:
        hlwm.get_attr(f'clients.{winid}.pseudotile') == hlwm.bool(True)


def test_apply_tmp_rule_move_to_tag(hlwm):
    client, _ = hlwm.create_client()
    hlwm.call('add foo')
    hlwm.call('add bar')
    target = 'foo'
    assert hlwm.get_attr(f'clients.{client}.tag') != target

    hlwm.call(f'apply_tmp_rule {client} tag={target}')

    assert hlwm.get_attr(f'clients.{client}.tag') == target


def test_apply_tmp_rule_focus(hlwm):
    oldfocus, _ = hlwm.create_client()
    newfocus, _ = hlwm.create_client()
    hlwm.call(f'jumpto {oldfocus}')
    assert hlwm.get_attr('clients.focus.winid') == oldfocus

    hlwm.call(f'apply_tmp_rule {newfocus} focus=on')

    assert hlwm.get_attr('clients.focus.winid') == newfocus


def test_apply_tmp_rule_parse_error(hlwm, hlwm_process):
    hlwm.create_client()
    hlwm.call_xfail('apply_tmp_rule --all focus=not-a-bool') \
        .expect_stderr('Invalid argument "not-a-bool" to consequence "focus"')


def test_smart_placement_within_monitor(hlwm):
    """
    In the smart placement, the outer geometry of clients
    must be used. So test that even with wide decorations,
    the top left corner of the decoration is not off screen.
    """
    hlwm.attr.tags.focus.floating = 'on'
    hlwm.call('rule floatplacement=smart')
    bw = 25
    hlwm.attr.theme.border_width = bw
    hlwm.attr.settings.snap_gap = 5  # something smaller than bw
    winid, _ = hlwm.create_client()

    inner_geometry = hlwm.attr.clients[winid].content_geometry()

    # assert that the top left corner of the decoration
    # is still within the monitor
    assert inner_geometry.x - bw >= 0
    assert inner_geometry.y - bw >= 0


@pytest.mark.parametrize('command', ['rule', 'apply_rules', 'apply_tmp_rule'])
@pytest.mark.parametrize('visible_tag', [True, False])
def test_floating_geometry(hlwm, x11, command, visible_tag):
    if command != 'rule':
        _, winid = x11.create_client()  # client first

    geo = Rectangle(30, 40, 140, 170)
    rule = ['floating=on', 'focus=on', 'floating_geometry=' + geo.to_user_str()]

    if not visible_tag:
        hlwm.call('add othertag')
        rule += ['tag=othertag']

    if command == 'rule':
        hlwm.call(['rule'] + rule)
        _, winid = x11.create_client()  # client second
    elif command == 'apply_rules':
        # apply fresh rules to long existing client
        hlwm.call(['rule'] + rule)
        hlwm.call(['apply_rules', winid])
    else:
        hlwm.call(['apply_tmp_rule', winid] + rule)

    if not visible_tag:
        hlwm.call('use othertag')

    assert hlwm.attr.clients.focus.floating_geometry() == geo
    assert hlwm.attr.clients.focus.content_geometry() == geo


@pytest.mark.parametrize('place_center', [True, False])
def test_floating_geometry_and_placement(hlwm, x11, place_center):
    hlwm.attr.tags.focus.floating = True

    rule_geo = Rectangle(30, 40, 140, 170)
    rule = ['floating_geometry=' + rule_geo.to_user_str()]
    if place_center:
        rule += ['floatplacement=center']
    else:
        # also add another rule whose floatplacement is then overwritten:
        hlwm.call('rule floatplacement=center')
        rule += ['floatplacement=none']

    # 'floatplacement' is only applied by 'rule', so no test
    # for 'apply_tmp_rule' or 'apply_rules'
    hlwm.call(['rule'] + rule)
    _, winid = x11.create_client()

    client_geo = hlwm.attr.clients[winid].floating_geometry()
    monitor_geo = hlwm.attr.monitors.focus.geometry()

    assert client_geo.size() == rule_geo.size()
    if place_center:
        assert client_geo.center() == monitor_geo.center()
    else:
        assert client_geo.topleft() == rule_geo.topleft()


def test_fixedsize_takes_no_operator(hlwm):
    hlwm.call_xfail('rule fixedsize=on') \
        .expect_stderr('Unknown argument "fixedsize=on"')


def test_fixedsize(hlwm, hc_idle, x11):
    hlwm.call('rule fixedsize hook=fixedsize floating=on')
    hlwm.call('rule not fixedsize hook=nofixedsize floating=off')
    hc_idle.hooks()  # clear hooks

    def make_fixedsize(w):
        hints = {
            'min_width': 400,
            'min_height': 300,
            'max_width': 400,
            'max_height': 300,
            'flags': Xutil.PMinSize | Xutil.PMaxSize,
        }
        w.set_wm_normal_hints(**hints)

    _, win_fixed = x11.create_client(pre_map=make_fixedsize)
    _, win_nofixed = x11.create_client()

    assert hlwm.attr.clients[win_fixed].floating() is True
    assert hlwm.attr.clients[win_nofixed].floating() is False

    hooks = hc_idle.hooks()
    assert ['rule', 'fixedsize', win_fixed] in hooks
    assert ['rule', 'nofixedsize', win_fixed] not in hooks
    assert ['rule', 'nofixedsize', win_nofixed] in hooks
    assert ['rule', 'fixedsize', win_nofixed] not in hooks
