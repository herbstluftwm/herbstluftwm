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
    'pseudotile',
    'ewmhrequests',
    'ewmhnotify',
    'fullscreen',
    'hook',
    'keymask',
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


@pytest.mark.parametrize('value', ['true', 'false'])
def test_pseudotile_consequence(hlwm, value):
    hlwm.call('rule pseudotile=' + value)

    hlwm.create_client()

    assert hlwm.get_attr('clients.focus.pseudotile') == value


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


@pytest.mark.parametrize(
    'name',
    ['pseudotile', 'fullscreen', 'ewmhrequests', 'ewmhnotify', 'fullscreen'])
@pytest.mark.parametrize('value', [True, False])
def test_bool_consequence_with_corresponding_attribute(hlwm, name, value):
    hlwm.call(['rule', name + '=' + hlwm.bool(value)])
    winid, _ = hlwm.create_client()

    assert hlwm.get_attr('clients.{}.{}'.format(winid, name)) == hlwm.bool(value)


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
