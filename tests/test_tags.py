import pytest

from conftest import PROCESS_SHUTDOWN_TIME


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


def test_add_tag_empty(hlwm):
    hlwm.call_xfail(['add', '']) \
        .expect_stderr('An empty tag name is not permitted')


def test_add_tag_completion(hlwm):
    hlwm.command_has_all_args(['add', 'foo'])


def test_add_tag_duplicate(hlwm):
    assert hlwm.attr.tags.count() == 1
    hlwm.call('add foo')
    assert hlwm.attr.tags.count() == 2
    hlwm.call('add bar')
    assert hlwm.attr.tags.count() == 3
    hlwm.call('add foo')
    assert hlwm.attr.tags.count() == 3


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


@pytest.mark.parametrize("move_command", ["move", "move_index"])
def test_move_focused_client_to_new_tag(hlwm, move_command):
    hlwm.call('add foobar')
    assert hlwm.get_attr('tags.0.client_count') == '0'
    assert hlwm.get_attr('tags.1.client_count') == '0'

    winid, _ = hlwm.create_client()
    assert hlwm.get_attr('tags.0.client_count') == '1'
    assert hlwm.get_attr('tags.1.client_count') == '0'

    if move_command == "move":
        hlwm.call('move foobar')
    elif move_command == "move_index":
        target_tag_idx = hlwm.get_attr('tags.by-name.foobar.index')
        hlwm.call(f'move_index {target_tag_idx}')

    assert hlwm.get_attr('tags.0.client_count') == '0'
    assert hlwm.get_attr('tags.0.curframe_wcount') == '0'
    assert hlwm.get_attr('tags.1.client_count') == '1'
    assert hlwm.get_attr('tags.1.curframe_wcount') == '1'
    assert hlwm.get_attr('clients', winid, 'tag') == 'foobar'


def test_move_focused_client_by_relative_index(hlwm):
    hlwm.call('add foobar')
    hlwm.call('add baz')
    assert hlwm.get_attr('tags.0.client_count') == '0'
    assert hlwm.get_attr('tags.1.client_count') == '0'
    assert hlwm.get_attr('tags.2.client_count') == '0'

    winid, _ = hlwm.create_client()
    assert hlwm.get_attr('tags.0.client_count') == '1'
    assert hlwm.get_attr('tags.1.client_count') == '0'
    assert hlwm.get_attr('tags.2.client_count') == '0'

    hlwm.call('move_index -1')

    assert hlwm.get_attr('tags.0.client_count') == '0'
    assert hlwm.get_attr('tags.1.client_count') == '0'
    assert hlwm.get_attr('tags.2.client_count') == '1'
    assert hlwm.get_attr('clients', winid, 'tag') == 'baz'

    hlwm.call('use_index -1')
    hlwm.call('move_index +2')

    assert hlwm.get_attr('tags.0.client_count') == '0'
    assert hlwm.get_attr('tags.1.client_count') == '1'
    assert hlwm.get_attr('tags.2.client_count') == '0'
    assert hlwm.get_attr('clients', winid, 'tag') == 'foobar'


def test_move_index_invalid_index(hlwm):
    hlwm.call_xfail('move_index 4711') \
        .expect_stderr('Invalid index "4711"')


def test_merge_tag_into_another_tag(hlwm):
    hlwm.call('add foobar')
    hlwm.create_client()
    hlwm.call('use_index 1')

    hlwm.call('merge_tag default foobar')

    assert hlwm.get_attr('tags.count') == '1'
    assert hlwm.get_attr('tags.0.index') == '0'
    assert hlwm.get_attr('tags.0.name') == 'foobar'


def test_merge_tag_invalid_arg(hlwm):
    hlwm.call('add othertag')
    hlwm.call('add anothertag')
    hlwm.call('add_monitor 800x600+600+0 othertag')

    hlwm.call_xfail('merge_tag othertag anothertag') \
        .expect_stderr('Cannot.*currently viewed')

    hlwm.call_xfail('merge_tag othertag nonexisting') \
        .expect_stderr('no such tag: nonexisting')

    hlwm.call_xfail('merge_tag nonexisting othertag') \
        .expect_stderr('no such tag: nonexisting')


@pytest.mark.parametrize("tag_count, old_idx, new_idx", [
    (count, old, new)
    for count in range(0, 6)
    for old in range(0, count)
    for new in range(0, count)])
def test_index_change(hlwm, tag_count, old_idx, new_idx):
    names = ["orig" + str(i) for i in range(0, tag_count)]
    for n in names:
        hlwm.call(['add', n])

    # get rid of the 'default' tag
    hlwm.call('use_index 1')
    hlwm.call('merge_tag default')

    hlwm.call(f'attr tags.{old_idx}.index {new_idx}')

    assert hlwm.get_attr(f'tags.{new_idx}.name') == names[old_idx]
    new_names = [n for n in names if n != names[old_idx]]
    new_names.insert(new_idx, names[old_idx])
    for i in range(0, tag_count):
        assert hlwm.get_attr(f'tags.{i}.index') == str(i)
        assert hlwm.get_attr(f'tags.{i}.name') == new_names[i]


@pytest.mark.parametrize("tag_count", [1, 4])
def test_index_to_big(hlwm, tag_count):
    for i in range(1, tag_count):  # one tag already exists
        hlwm.call(f'add tag{i}')

    for new_idx in [tag_count, tag_count + 10]:
        hlwm.call_xfail(f'attr tags.0.index {new_idx}') \
            .expect_stderr(f'Index must be between 0 and {tag_count - 1}')


RENAMING_COMMANDS = [
    # commands for renaming the default tag
    lambda old, new: ['set_attr', 'tags.by-name.{}.name'.format(old), new],
    lambda old, new: ['rename', old, new]]


@pytest.mark.parametrize("rename_command", RENAMING_COMMANDS)
def test_rename_tag(hlwm, hc_idle, rename_command):
    hlwm.call(rename_command('default', 'foobar'))

    assert hlwm.get_attr('tags.0.name') == 'foobar'
    assert hc_idle.hooks() == [['tag_renamed', 'default', 'foobar']]

    hlwm.call(rename_command('foobar', 'baz'))

    assert hlwm.get_attr('tags.0.name') == 'baz'
    assert hc_idle.hooks() == [['tag_renamed', 'foobar', 'baz']]


@pytest.mark.parametrize("rename_command", RENAMING_COMMANDS)
def test_rename_tag_empty(hlwm, rename_command):
    hlwm.call_xfail(rename_command('default', '')) \
        .expect_stderr('An empty tag name is not permitted')


@pytest.mark.parametrize("rename_command", RENAMING_COMMANDS)
def test_rename_tag_existing_tag(hlwm, rename_command):
    hlwm.call('add foobar')

    hlwm.call_xfail(rename_command('default', 'foobar')) \
        .expect_stderr('"foobar" already exists')


def test_rename_non_existing_tag(hlwm):
    hlwm.call_xfail(['rename', 'foobar', 'baz']) \
        .expect_stderr('no such tag: foobar')


def test_floating_invalid_parameter(hlwm):
    # passing a non-boolean must be handled
    hlwm.call_xfail('floating invalidvalue') \
        .expect_stderr('invalid argument')


@pytest.mark.parametrize("tiled_num", [3])
@pytest.mark.parametrize("floated_num", [2])
def test_client_count_attribute(hlwm, tiled_num, floated_num):
    hlwm.create_clients(tiled_num)
    floated = hlwm.create_clients(floated_num)
    for winid in floated:
        hlwm.call(f'attr clients.{winid}.floating true')

    assert int(hlwm.get_attr('tags.focus.client_count')) \
        == tiled_num + floated_num


@pytest.mark.parametrize("command", [
    "close_or_remove",
    "close_and_remove",
])
def test_close_and_or_remove_floating(hlwm, command):
    # set up some empty frames and a floating client
    hlwm.call('split explode')
    winid, proc = hlwm.create_client()
    hlwm.call(f'set_attr clients.{winid}.floating true')
    hlwm.call(f'jumpto {winid}')
    assert hlwm.get_attr('clients.focus.winid') == winid
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 2

    # run close_or_remove / close_and_remove
    hlwm.call(command)

    # in any case no frame may have been removed
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 2
    # and the client is closed:
    proc.wait(PROCESS_SHUTDOWN_TIME)


def test_close_and_remove_with_one_client(hlwm):
    hlwm.call('split explode')
    winid, proc = hlwm.create_client()
    assert hlwm.get_attr('clients.focus.winid') == winid
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 2

    hlwm.call('close_and_remove')

    # this closes the client and removes the frame
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 1
    proc.wait(PROCESS_SHUTDOWN_TIME)


def test_close_and_remove_with_two_clients(hlwm):
    hlwm.call('split explode')
    winid, proc = hlwm.create_client()
    other_winid, _ = hlwm.create_client()
    assert hlwm.get_attr('clients.focus.winid') == winid
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 2

    hlwm.call('close_and_remove')

    # this closes the client, but does not remove the frame
    # since there is a client left
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 2
    proc.wait(PROCESS_SHUTDOWN_TIME)


def test_close_and_remove_without_clients(hlwm):
    hlwm.call('split explode')
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 2

    hlwm.call('close_and_remove')

    # this acts like remove:
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 1


def test_close_or_remove_client(hlwm):
    # This is like close_and_remove, but requires hitting
    # 'close_or_remove' twice.
    hlwm.call('split explode')
    winid, proc = hlwm.create_client()
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 2

    # On the first invocation:
    hlwm.call('close_or_remove')
    # only close the client
    proc.wait(PROCESS_SHUTDOWN_TIME)
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 2

    # On the second invocation:
    hlwm.call('close_or_remove')
    # remove the frame
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 1


@pytest.mark.parametrize("floated_num", [1, 2])
def test_floating_focused_activate(hlwm, floated_num):
    # create a floating client which is not focused
    # because the tiling layer is focused
    hlwm.call('rule floating=on focus=off')
    floating_winid, _ = hlwm.create_client()
    if floated_num > 1:
        # create another floating client that does not get the focus
        hlwm.create_client()
    assert hlwm.get_attr('tags.focus.floating_focused') == hlwm.bool(False)
    assert 'focus' not in hlwm.list_children('clients')

    # switch to floating layer
    hlwm.call('attr tags.focus.floating_focused on')

    # the floating client is focused
    assert hlwm.get_attr('tags.focus.floating_focused') == hlwm.bool(True)
    assert hlwm.get_attr('clients.focus.winid') == floating_winid


def test_floating_focused_deactivate(hlwm):
    # create one floating client, and focus it
    hlwm.call('rule floating=on focus=on')
    floating_winid, _ = hlwm.create_client()
    assert hlwm.get_attr('tags.focus.floating_focused') == hlwm.bool(True)
    assert hlwm.get_attr('clients.focus.winid') == floating_winid

    # switch to tiling layer
    hlwm.call('attr tags.focus.floating_focused off')

    assert hlwm.get_attr('tags.focus.floating_focused') == hlwm.bool(False)
    assert 'focus' not in hlwm.list_children('clients')


def test_floating_focused_vacouus(hlwm):
    assert hlwm.get_attr('tags.focus.floating_focused') == hlwm.bool(False)

    # switching to floating layer should not be possible if
    # there is no floating window
    hlwm.call_xfail('attr tags.focus.floating_focused on') \
        .expect_stderr(r'There are no \(non-minimized\) floating windows')


@pytest.mark.parametrize("tag", [True, False])
@pytest.mark.parametrize("single", [True, False])
def test_floating_correct_position(hlwm, single, tag):
    if single:
        hlwm.call('rule floating=on')
    if tag:
        hlwm.attr.tags.focus.floating = 'on'

    winid, _ = hlwm.create_client()
    hlwm.call('move_monitor "" 800x600+0+0')
    hlwm.call('pad "" 0 0 0 0')

    clientobj = hlwm.attr.clients[winid]

    # the floating geometry is used iff tag or client is set to floating:
    floating_geom_applied = single or tag
    assert (clientobj.floating_geometry() == clientobj.content_geometry()) \
        == floating_geom_applied


def test_urgent_count(hlwm, x11):
    # create 5 urgent clients
    for i in range(0, 5):
        x11.create_client(urgent=True)

    # since one of them gets focused, 4 urgent clients remain
    assert int(hlwm.get_attr('tags.focus.urgent_count')) == 4


def test_rename_multiple_tags(hlwm, hc_idle):
    hlwm.call('add foo_old')
    hlwm.call('add bar_old')
    hc_idle.hooks()  # clear hooks

    hlwm.call('rename foo_old foo_new')
    hlwm.call('rename bar_old bar_new')

    assert hc_idle.hooks() == [
        ['tag_renamed', 'foo_old', 'foo_new'],
        ['tag_renamed', 'bar_old', 'bar_new']
    ]


# the test cases on focused_client are implicitly tests
# for the DynChild_ related code.
@pytest.mark.parametrize("client_exists", [True, False])
def test_focused_client_existence(hlwm, client_exists):
    # here, I want the same python code for the positive
    # and negative test, because the negative test only checks
    # whether no entry exists.
    if client_exists:
        winid, _ = hlwm.create_client()

    def test_children(children):
        assert ('focused_client' in children) == client_exists
        assert 'tiling' in children

    test_children(hlwm.list_children_via_attr('tags.0'))
    test_children(hlwm.list_children('tags.0'))

    if client_exists:
        assert hlwm.get_attr('tags.0.focused_client.winid') == winid


def test_focused_client_multiple_tags(hlwm):
    tagname2clientcount = [
        ('t1', 1),
        ('t2', 0),
        ('t3', 3),
        ('t4', 1),
    ]
    tagname2clients = {}
    for t, cnt in tagname2clientcount:
        # replace dict entry by client list
        hlwm.call(f'chain , add {t} , rule tag={t} focus=off')
        tagname2clients[t] = [hlwm.create_client()[0] for _ in range(0, cnt)]

    for t, clients in tagname2clients.items():
        assert ('focused_client' in hlwm.list_children(f'tags.by-name.{t}')) \
            == (len(clients) > 0)
        if len(clients) > 0:
            assert hlwm.get_attr(f'tags.by-name.{t}.focused_client.winid') \
                == clients[0]


def test_minimized_window_stays_invisible_on_tag_change(hlwm):
    hlwm.call('add othertag')
    hlwm.call('rule tag=othertag')
    winid, _ = hlwm.create_client()
    hlwm.call(f'set_attr clients.{winid}.minimized true')
    assert hlwm.get_attr(f'clients.{winid}.floating') == 'false'
    assert hlwm.get_attr(f'clients.{winid}.tag') == 'othertag'
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'false'

    this_tag = 'default'
    # bring the window to this_tag without focusing it
    hlwm.call(['unrule', '--all'])
    hlwm.call(['rule', 'tag=' + this_tag])
    hlwm.call(['apply_rules', winid])
    assert hlwm.get_attr(f'clients.{winid}.tag') == this_tag

    # after bringing the client to a visible tag, it must stay
    # invisible
    assert hlwm.get_attr(f'clients.{winid}.minimized') == 'true'
    assert hlwm.get_attr(f'clients.{winid}.visible') == 'false'
    # and the minimized client is not in the tiling layer!
    assert hlwm.get_attr('tags.focus.tiling.root.client_count') == '0'
    assert hlwm.get_attr(f'clients.{winid}.floating') == 'false'


def test_minimized_window_stays_minimized_on_tag_change(hlwm):
    hlwm.call('add othertag')
    hlwm.call('rule tag=othertag')
    winid, _ = hlwm.create_client()
    hlwm.call(f'set_attr clients.{winid}.minimized true')
    assert hlwm.get_attr(f'clients.{winid}.floating') == 'false'

    this_tag = 'default'
    # bring the window to this_tag without focusing it
    hlwm.call(['unrule', '--all'])
    hlwm.call(['rule', 'tag=' + this_tag])
    hlwm.call(['apply_rules', winid])
    assert hlwm.get_attr(f'clients.{winid}.tag') == this_tag

    # the minimized client is not in the tiling layer
    assert hlwm.get_attr('tags.focus.tiling.root.client_count') == '0'
    assert hlwm.get_attr(f'clients.{winid}.floating') == 'false'
    assert hlwm.get_attr(f'clients.{winid}.minimized') == 'true'
    # but after un-minimizing it, it is
    hlwm.call(f'set_attr clients.{winid}.minimized false')
    assert hlwm.get_attr('tags.focus.tiling.root.client_count') == '1'


def test_merge_tag_minimized_into_visible_tag(hlwm):
    hlwm.call('add source')
    hlwm.call('add target')
    hlwm.call('use source')
    winid, _ = hlwm.create_client()
    hlwm.attr.clients[winid].minimized = hlwm.bool(True)
    hlwm.call('use target')

    assert hlwm.attr.tags['by-name'].target.visible() is True
    hlwm.call('merge_tag source')

    assert hlwm.attr.clients[winid].visible() is False
    assert winid not in hlwm.call('dump').stdout
