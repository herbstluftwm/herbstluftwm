import pytest


def test_emit_hook(hlwm, hc_idle):
    hlwm.call('emit_hook my_hook')
    assert hc_idle.hooks() == [['my_hook']]


def test_hc_idle_fixture_resets(hlwm, hc_idle):
    hlwm.call('emit_hook my_hook a')
    assert hc_idle.hooks() == [['my_hook', 'a']]
    hlwm.call('emit_hook my_hook2 b')
    assert hc_idle.hooks() == [['my_hook2', 'b']]


def test_hc_idle_fixture_collects(hlwm, hc_idle):
    hlwm.call('emit_hook my_hook a')
    hlwm.call('emit_hook my_hook2 b c')
    assert hc_idle.hooks() == [['my_hook', 'a'], ['my_hook2', 'b', 'c']]


def test_fullscreen_change_emits_hook(hlwm, hc_idle):
    hlwm.call('rule fullscreen=on')
    winid, _ = hlwm.create_client()
    assert ['fullscreen', 'on', str(winid)] in hc_idle.hooks()

    hlwm.call('set_attr clients.focus.fullscreen off')
    assert ['fullscreen', 'off', str(winid)] in hc_idle.hooks()


@pytest.mark.parametrize("change", [True, False])
def test_no_fullscreen_hook_if_nothing_changes(hlwm, hc_idle, change):
    if change:
        hlwm.call('rule fullscreen=on')
    winid, _ = hlwm.create_client()

    # a fullscreen hook is fired if and only if a proper change happens
    assert ('fullscreen' in [hook[0] for hook in hc_idle.hooks()]) == change
