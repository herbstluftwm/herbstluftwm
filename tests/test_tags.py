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
    assert hlwm.get_attr('tags.0.index') == '0'
    assert hlwm.get_attr('tags.0.name') == 'foobar'


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
    proc.wait(10)


def test_close_and_remove_with_one_client(hlwm):
    hlwm.call('split explode')
    winid, proc = hlwm.create_client()
    assert hlwm.get_attr('clients.focus.winid') == winid
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 2

    hlwm.call('close_and_remove')

    # this closes the client and removes the frame
    assert int(hlwm.get_attr('tags.focus.frame_count')) == 1
    proc.wait(10)


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
    proc.wait(10)


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
    proc.wait(10)
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
        .expect_stderr("There are no floating windows")


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
