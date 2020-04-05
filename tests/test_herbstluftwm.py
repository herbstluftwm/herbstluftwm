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
