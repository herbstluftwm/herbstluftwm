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
    # FIXME: Is there a more robust way to find the default autostart path?
    default_autostart = os.path.join(os.path.abspath(BINDIR), '..', 'share/autostart')
    autostart = subprocess.Popen(['/usr/bin/env', 'bash', '-e', default_autostart],
                                 stderr=subprocess.PIPE,
                                 stdout=subprocess.PIPE,
                                 universal_newlines=True)
    autostart.wait(2)
    assert autostart.returncode == 0
    assert hlwm.list_children('tags.by-name') == sorted(expected_tags)
    # Test a random setting different from the default in settings.h:
    assert hlwm.get_attr('settings.smart_frame_surroundings') == 'true'
