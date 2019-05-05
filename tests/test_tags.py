import pytest


def test_default_tag_exists_and_has_name(hlwm):
    assert hlwm.get_attr('tags.count') == '1'
    assert hlwm.get_attr('tags.0.name') == 'default'


def test_add_tag(hlwm):
    focus_before = hlwm.get_attr('tags.focus.name')

    hlwm.call('add foobar')

    assert hlwm.get_attr('tags.count') == '2'
    assert hlwm.get_attr('tags.1.client_count') == '0'
    assert hlwm.get_attr('tags.1.client_count') == '0'
    assert hlwm.get_attr('tags.1.curframe_wcount') == '0'
    assert hlwm.get_attr('tags.1.curframe_windex') == '0'
    assert hlwm.get_attr('tags.1.frame_count') == '1'
    assert hlwm.get_attr('tags.1.index') == '1'
    assert hlwm.get_attr('tags.1.name') == 'foobar'
    assert hlwm.get_attr('tags.focus.name') == focus_before


def test_use_tag(hlwm):
    assert hlwm.get_attr('tags.focus.index') == '0'
    hlwm.call('add foobar')

    hlwm.call('use foobar')

    assert hlwm.get_attr('tags.focus.index') == '1'
    assert hlwm.get_attr('tags.focus.name') == 'foobar'


def test_use_previous(hlwm):
    hlwm.call('add foobar')
    hlwm.call('use foobar')
    assert hlwm.get_attr('tags.focus.index') == '1'

    hlwm.call('use_previous')

    assert hlwm.get_attr('tags.focus.index') == '0'

    hlwm.call('use_previous')

    assert hlwm.get_attr('tags.focus.index') == '1'


@pytest.mark.parametrize("running_clients_num", [0, 1, 5])
def test_new_clients_increase_client_count(hlwm, running_clients, running_clients_num):
    assert hlwm.get_attr('tags.0.client_count') == str(running_clients_num)


def test_move_focused_client_to_new_tag(hlwm):
    hlwm.call('add foobar')
    assert hlwm.get_attr('tags.0.client_count') == '0'
    assert hlwm.get_attr('tags.1.client_count') == '0'

    winid, _ = hlwm.create_client()
    assert hlwm.get_attr('tags.0.client_count') == '1'
    assert hlwm.get_attr('tags.1.client_count') == '0'

    hlwm.call('move foobar')

    assert hlwm.get_attr('tags.0.client_count') == '0'
    assert hlwm.get_attr('tags.0.curframe_wcount') == '0'
    assert hlwm.get_attr('tags.1.client_count') == '1'
    assert hlwm.get_attr('tags.1.curframe_wcount') == '1'
    assert hlwm.get_attr('clients', winid, 'tag') == 'foobar'


def test_merge_tag_into_another_tag(hlwm):
    hlwm.call('add foobar')
    hlwm.create_client()
    hlwm.call('use_index 1')

    hlwm.call('merge_tag default foobar')

    assert hlwm.get_attr('tags.count') == '1'
    assert hlwm.get_attr('tags.0.name') == 'foobar'


RENAMING_COMMANDS = [
    # commands for renaming the default tag
    ['set_attr', 'tags.by-name.default.name'],
    ['rename', 'default']]


@pytest.mark.parametrize("rename_command", RENAMING_COMMANDS)
def test_rename_tag(hlwm, hc_idle, rename_command):
    hlwm.call(rename_command + ['foobar'])

    assert hlwm.get_attr('tags.0.name') == 'foobar'
    assert hc_idle.hooks() == [['tag_renamed', 'foobar']]


@pytest.mark.parametrize("rename_command", RENAMING_COMMANDS)
def test_rename_tag_empty(hlwm, rename_command):
    hlwm.call_xfail(rename_command + [""]) \
        .expect_stderr('An empty tag name is not permitted')


@pytest.mark.parametrize("rename_command", RENAMING_COMMANDS)
def test_rename_tag_existing_tag(hlwm, rename_command):
    hlwm.call('add foobar')

    hlwm.call_xfail(rename_command + ["foobar"]) \
        .expect_stderr('"foobar" already exists')
