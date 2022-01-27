import pytest
import conftest
from herbstluftwm.types import Rectangle
from conftest import MultiscreenDisplay


@pytest.mark.parametrize('screens', [
    [(500, 450), (300, 400)],
    [(500, 450), (300, 600), (200, 600)],
])
@pytest.mark.parametrize('server_type', ['Xvfb', 'Xephyr'])
def test_detect_monitors_xinerama(hlwm_spawner, server_type, screens, xvfb):
    args = ['+extension', 'XINERAMA']
    args += ['-extension', 'RANDR']
    with MultiscreenDisplay(server=server_type, screens=screens, extra_args=args) as xserver:
        # boot up hlwm
        hlwm_proc = hlwm_spawner(display=xserver.display)
        hlwm = conftest.HlwmBridge(xserver.display, hlwm_proc)
        # create enough tags
        hlwm.call('chain , add tag1 , add tag2 , add tag3')
        assert hlwm.get_attr('monitors.count') == '1'

        hlwm.call('detect_monitors --no-disjoin')
        # for the debugging if something fails:
        hlwm.call('detect_monitors --list-all')

        # check number of monitors
        assert len(screens) == len(xserver.screens())
        assert hlwm.get_attr('monitors.count') == str(len(screens))
        # check the position and size of monitors
        for idx, expected_geo in enumerate(xserver.screens()):
            monrect = hlwm.call(['monitor_rect', idx]).stdout.split(' ')
            assert expected_geo == [int(v) for v in monrect]

        hlwm_proc.shutdown()


@pytest.mark.parametrize('screens', [
    [(500, 450), (300, 600), (300, 600)],
    [(500, 450), (300, 600), (200, 100), (300, 600), (300, 600)],
])
def test_detect_monitors_deduplication(hlwm_spawner, screens, xvfb):
    args = ['+extension', 'XINERAMA']
    args += ['-extension', 'RANDR']
    # Xvfb puts all screens at position (0,0),
    # so we use it to verify that detect_monitors removes duplicates
    # while preserving the order
    with MultiscreenDisplay(server='Xvfb', screens=screens, extra_args=args) as xserver:
        # boot up hlwm
        hlwm_proc = hlwm_spawner(display=xserver.display)
        hlwm = conftest.HlwmBridge(xserver.display, hlwm_proc)
        # create enough tags
        hlwm.call('chain , add tag1 , add tag2 , add tag3')
        assert hlwm.get_attr('monitors.count') == '1'
        rects = hlwm.call(['detect_monitors', '-l']).stdout.splitlines()
        expected = [
            Rectangle(0, 0, s[0], s[1]).to_user_str()
            for idx, s in enumerate(screens) if screens.index(s) == idx]

        assert rects == expected

        hlwm_proc.shutdown()


@pytest.mark.parametrize('screens', [
    [(500, 450), (300, 400)],
    # this test only works for multiple screens,
    # because otherwise, xinerama is secretly activated
])
def test_detect_monitors_xrandr(hlwm_spawner, screens, xvfb):
    args = ['-extension', 'XINERAMA']
    args += ['+extension', 'RANDR']
    with MultiscreenDisplay(server='Xephyr', screens=screens, extra_args=args) as xserver:
        # boot up hlwm
        hlwm_proc = hlwm_spawner(display=xserver.display)
        hlwm = conftest.HlwmBridge(xserver.display, hlwm_proc)

        # for the debugging if something fails:
        lines = hlwm.call('detect_monitors --list-all').stdout.splitlines()
        for cur_line in lines:
            words = cur_line.split(' ')
            if words[0] == 'xinerama:':
                # check that xinerama is disabled
                assert len(words) == 1
            elif words[0] == 'xrandr:':
                # unfortunately, xrandr in Xephyr only reports the first screen
                assert len(words) == 2
                assert words[1] == Rectangle(x=0, y=0, width=screens[0][0], height=screens[0][1]).to_user_str()
            else:
                assert False, f"unknown detect_monitors output: {cur_line}"

        hlwm_proc.shutdown()
