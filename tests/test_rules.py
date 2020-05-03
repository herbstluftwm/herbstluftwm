import pytest


string_props = [
    'instance',
    'class',
    'title',
    'windowtype',
    'windowrole',
]

numeric_props = [
    'pid',
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
    'pseudotile',
    'ewmhrequests',
    'ewmhnotify',
    'fullscreen',
    'hook',
    'keymask',
    'keys_inactive',
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
    # Add set of rules with every consequence and every valid combination of
    # property and match operator appearing at least once:

    # Make a single, long list of all consequences (with unique rhs values):
    consequences_str = \
        ' '.join(['{}=a{}b'.format(c, idx)
                  for idx, c in enumerate(consequences, start=4117)])

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


def test_add_rule_with_misformatted_argument(hlwm):
    call = hlwm.call_xfail('rule notevenanoperator')

    call.expect_stderr('rule: No operator in given arg: notevenanoperator')


def test_cannot_add_rule_with_empty_label(hlwm):
    call = hlwm.call_xfail('rule label= class=Foo tag=bar')

    assert call.stderr == 'rule: Rule label cannot be empty\n'


def test_cannot_use_tilde_operator_for_rule_label(hlwm):
    call = hlwm.call_xfail('rule label~bla class=Foo tag=bar')

    assert call.stderr == 'rule: Unknown rule label operation "~"\n'


def test_add_rule_with_unknown_condition(hlwm):
    call = hlwm.call_xfail('rule foo=bar quit')
    call.expect_stderr('rule: Unknown argument "foo=bar"')


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
    call = hlwm.call_xfail('unrule nope')

    assert call.stderr == 'Couldn\'t find any rules with label "nope"'


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
        [i + ' ' for i in '! not prepend once printlabel'.split(' ')]
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

    assert call.stderr == 'rule: Can not parse value "[b-a]" from condition "class": "Invalid range in bracket expression."\n'


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


@pytest.mark.parametrize('negation', ['not', '!'])
def test_condition_must_come_after_negation(hlwm, negation):
    call = hlwm.call_xfail(['rule', negation])

    assert call.stderr == f'Expected another argument after "{negation}" flag\n'


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


def test_consequence_invalid_argument(hlwm):
    # TODO: make this command fail at some point:
    hlwm.call('rule focus=not-a-boolean')

    # this must not crash:
    hlwm.create_client()


def create_client(hlwm, client_before_rule, rule):
    """create a client and the given rule. if client_before_rules, then
    first create the client, and later apply the rule to it via the
    apply_rule command"""
    if client_before_rule:
        winid, _ = hlwm.create_client()
        hlwm.call(['rule'] + rule)
        hlwm.call(['apply_rules', winid])
    else:
        hlwm.call(['rule'] + rule)
        winid, _ = hlwm.create_client()
    return winid


@pytest.mark.parametrize(
    'name',
    ['floating', 'pseudotile', 'fullscreen', 'ewmhrequests', 'ewmhnotify', 'fullscreen'])
@pytest.mark.parametrize('value', [True, False])
@pytest.mark.parametrize('apply_rules', [True, False])
def test_bool_consequence_with_corresponding_attribute(hlwm, name, value, apply_rules):
    winid = create_client(hlwm, apply_rules, [name + '=' + hlwm.bool(value)])

    assert hlwm.get_attr('clients.{}.{}'.format(winid, name)) == hlwm.bool(value)


@pytest.mark.parametrize(
    'name',
    ['keymask', 'keys_inactive'])
@pytest.mark.parametrize('apply_rules', [True, False])
def test_regex_consequence_with_corresponding_attribute(hlwm, name, apply_rules):
    value = 'someregex'
    winid = create_client(hlwm, apply_rules, [name + '=' + value])

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


def test_apply_rules_all_focus_retained(hlwm):
    hlwm.call('rule focus=on')
    client1, _ = hlwm.create_client()
    client2, _ = hlwm.create_client()
    client3, _ = hlwm.create_client()
    # check that the focus is retained, no matter in which
    # order they appear in the hash_map:
    for client in [client1, client2, client3]:
        hlwm.call(['jumpto', client])
        assert hlwm.get_attr('clients.focus.winid') == client

        hlwm.call('apply_rules --all')

        assert hlwm.get_attr('clients.focus.winid') == client


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


def test_apply_rules_invalid_window(hlwm):
    hlwm.call_xfail(['apply_rules', 'invalidWindowId']) \
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
