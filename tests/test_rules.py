import pytest
import re


def test_list_rules_empty_by_default(hlwm):
    rules = hlwm.call('list_rules')

    assert rules.stdout == ''


def test_add_simple_rule(hlwm):
    hlwm.call('rule', 'class=Foo', 'tag=bar')

    rules = hlwm.call('list_rules')
    assert rules.stdout == 'label=0\tclass=Foo\ttag=bar\t\n'


def test_add_labeled_rule(hlwm):
    hlwm.call('rule', 'label=mylabel', 'class=Foo', 'tag=bar')

    rules = hlwm.call('list_rules')
    assert rules.stdout == 'label=mylabel\tclass=Foo\ttag=bar\t\n'


@pytest.mark.parametrize('method', ['-F', '--all'])
def test_remove_all_rules(hlwm, method):
    hlwm.call('rule', 'class=Foo', 'tag=bar')
    hlwm.call('rule', 'label=labeled', 'class=Bork', 'tag=baz')

    hlwm.call('unrule', method)

    rules = hlwm.call('list_rules')
    assert rules.stdout == ''


def test_remove_simple_rule(hlwm):
    hlwm.call('rule', 'class=Foo', 'tag=bar')

    hlwm.call('unrule', '0')

    rules = hlwm.call('list_rules')
    assert rules.stdout == ''


def test_remove_labeled_rule(hlwm):
    hlwm.call('rule', 'label=blah', 'class=Foo', 'tag=bar')

    hlwm.call('unrule', 'blah')

    rules = hlwm.call('list_rules')
    assert rules.stdout == ''
