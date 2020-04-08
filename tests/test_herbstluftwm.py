import re
import os
import subprocess
from conftest import BINDIR


HLWM_PATH = os.path.join(BINDIR, 'herbstluftwm')


def test_herbstluftwm_already_running(hlwm):
    result = subprocess.run([HLWM_PATH],
                            stderr=subprocess.PIPE,
                            universal_newlines=True)
    assert result.returncode == 1
    assert re.search(r'another window manager is already running', result.stderr)


def test_herbstluftwm_default_autostart(hlwm):
    expected_tags = [str(tag) for tag in range(1, 10)]
    default_autostart = os.path.join(os.path.abspath(BINDIR), 'share/autostart')
    env_with_bindir_path = os.environ.copy()
    env_with_bindir_path['PATH'] = BINDIR + ":" + env_with_bindir_path['PATH']
    subprocess.run(['bash', '-e', default_autostart], check=True, env=env_with_bindir_path)

    assert hlwm.list_children('tags.by-name') == sorted(expected_tags)
    # Test a random setting different from the default in settings.h:
    assert hlwm.get_attr('settings.smart_frame_surroundings') == 'true'
