import pytest
import conftest
from conftest import MultiscreenDisplay


@pytest.mark.parametrize('screens', [
    [(500, 450), (300, 400)],
    [(500, 450), (200, 600), (200, 600)],
])
@pytest.mark.parametrize('server_type', ['Xvfb', 'Xephyr'])
def test_detect_monitors_xinerama(hlwm_spawner, server_type, screens, xvfb):
    args = ['+extension', 'XINERAMA']
    args += ['-extension', 'RANDR']
    with MultiscreenDisplay(screens=screens, extra_args=args) as xserver:
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
