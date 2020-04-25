from datetime import datetime
from contextlib import contextmanager
from Xlib import X, Xutil, Xatom
import Xlib
import ewmh
import os
import os.path
import re
import select
import selectors
import shlex
import subprocess
import sys
import textwrap
import time
import types

import pytest
import warnings


BINDIR = os.path.join(os.path.abspath(os.environ['PWD']))


class HlwmBridge:

    HC_PATH = os.path.join(BINDIR, 'herbstclient')
    # if there is some HlwmBridge, then it is registered here:
    INSTANCE = None

    def __init__(self, display, hlwm_process):
        HlwmBridge.INSTANCE = self
        self.client_procs = []
        self.next_client_id = 0
        self.env = {
            'DISPLAY': display,
        }
        self.hlwm_process = hlwm_process
        self.hc_idle = subprocess.Popen(
            [self.HC_PATH, '--idle', 'rule', 'here_is_.*'],
            bufsize=1,  # line buffered
            universal_newlines=True,
            env=self.env,
            stdout=subprocess.PIPE
        )
        # a dictionary mapping wmclasses to window ids as reported
        # by self.hc_idle
        self.wmclass2winid = {}

    def _parse_command(self, cmd):
        """
        Parse a command (a string using shell quotes or
        a string list) to a string list.
        """
        if isinstance(cmd, list):
            args = [str(x) for x in cmd]
            assert args
        else:
            args = shlex.split(cmd)
        return args

    def unchecked_call(self, cmd, log_output=True, read_hlwm_output=True):
        """call the command but do not check exit code or stderr"""
        args = self._parse_command(cmd)

        try:
            proc = subprocess.run([self.HC_PATH, '-n'] + args,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  env=self.env,
                                  universal_newlines=True,
                                  # Kill hc when it hangs due to crashed server:
                                  timeout=2
                                  )
        except subprocess.TimeoutExpired:
            self.hlwm_process.investigate_timeout('calling ' + str(args))

        outcome = 'succeeded' if proc.returncode == 0 else 'failed'
        allout = proc.stdout + proc.stderr
        if allout:
            if log_output:
                print(f'Client command {args} {outcome} with output:\n{allout}')
            else:
                print(f'Client command {args} {outcome} with output', end='')
                print(' (output suppressed).')
        else:
            print(f'Client command {args} {outcome} (no output)')

        # Take this opportunity read and echo any hlwm output captured in the
        # meantime:
        if read_hlwm_output:
            self.hlwm_process.read_and_echo_output()

        return proc

    def call(self, cmd):
        """call the command and expect it to have exit code zero
        and no output on stderr"""
        proc = self.unchecked_call(cmd)
        assert proc.returncode == 0
        assert not proc.stderr
        return proc

    def call_xfail(self, cmd):
        """ Call the command, expect it to terminate with a non-zero exit code,
        emit no output on stdout but some output on stderr. The returned
        process handle offers a match() method that checks the stderr against a
        given regex. """
        proc = self.unchecked_call(cmd)
        assert proc.returncode != 0
        assert proc.stdout == ""
        assert proc.stderr != ""

        def f(self2, reg):
            assert re.search(reg, self2.stderr)

        proc.expect_stderr = types.MethodType(f, proc)
        return proc

    def call_xfail_no_output(self, cmd):
        """ Call the command, expect it to terminate with a non-zero exit code
        and emit no output on either stdout or stderr. """
        proc = self.unchecked_call(cmd)
        assert proc.returncode != 0
        assert proc.stdout == ""
        assert proc.stderr == ""
        return proc

    def get_attr(self, *attribute_path, check=True):
        """get an attribute where the given attribute_path arguments
        are joined with '.', so the following are equivalent:

            get_attr('clients', 'focus', 'title')
            get_attr('clients.focus', 'title')
            get_attr('clients.focus.title')
        """
        attribute_path = '.'.join([str(x) for x in attribute_path])
        return self.call(['get_attr', attribute_path]).stdout

    def create_client(self, term_command='sleep infinity', position=None, title=None, keep_running=False):
        """
        Launch a client that will be terminated on shutdown.
        """
        self.next_client_id += 1
        wmclass = 'client_{}'.format(self.next_client_id)
        title = ['-title', str(title)] if title else []
        geometry = ['-geometry', '50x20+0+0']
        if position is not None:
            x, y = position
            geometry[1] = '50x2%+d%+d' % (x, y)
        command = ['xterm'] + title + geometry
        command += ['-class', wmclass, '-e', 'bash', '-c', term_command]

        # enforce a hook when the window appears
        self.call(['rule', 'once', 'class=' + wmclass, 'hook=here_is_' + wmclass])

        proc = subprocess.Popen(command, env=self.env)
        # once the window appears, the hook is fired:
        winid = self.wait_for_window_of(wmclass)

        if not keep_running:
            # Add to list of processes to be killed on shutdown:
            self.client_procs.append(proc)

        return winid, proc

    def complete(self, cmd, partial=False, position=None, evaluate_escapes=False):
        """
        Return a sorted list of all completions for the next argument for the
        given command, if position=None. If position is given, then the
        argument of the given position is completed.

        Set 'partial' if some of the completions for the given command are
        partial. If not in 'partial' mode, trailing spaces are stripped.

        Set 'evaluate_escapes' if the escape sequences of completion items
        should be evaluated. If this is set, one can not distinguish between
        partial and full completion results anymore.
        """
        args = self._parse_command(cmd)
        if position is None:
            position = len(args)
        proc = self.call(['complete_shell', position] + args)
        items = []
        for i in proc.stdout.splitlines(False):
            if partial:
                items.append(i)
            else:
                if not i.endswith(' '):
                    raise Exception(("completion for \"{}\" returned the partial "
                                    + "completion item \"{}\"").format(cmd, i)
                                    ) from None
                else:
                    items.append(i[0:-1])
        # evaluate escape sequences:
        if evaluate_escapes:
            items = [shlex.split(s)[0] for s in items]
        return sorted(items)

    def list_children_via_attr(self, object_path):
        """
        List the names of children of the
        given object, using the attr command internally.
        """
        # regexes for list_children:

        children_re = \
            re.compile(r'^[0-9]* (child|children)[\\.:]((\n  [^\n]*)*)')
        line_re = re.compile('^  (.*)\\.$')
        output = self.call(['attr', object_path]).stdout
        section_match = children_re.match(output)
        assert section_match
        children = []
        for i in section_match.group(2).split('\n')[1:]:
            child_match = line_re.match(i)
            assert child_match
            children.append(child_match.group(1))
        return sorted(children)

    def list_children(self, object_path):
        """
        List the names of children of the
        given object, using the complete_shell command.
        """
        if not object_path.endswith('.') and object_path != '':
            object_path += '.'
        items = self.complete(['object_tree', object_path],
                              partial=True, position=1)
        children = []
        for i in items:
            children.append(i.split('.')[-2])
        return sorted(children)

    def create_clients(self, num):
        return [self.create_client()[0] for i in range(num)]

    def wait_for_window_of(self, wmclass):
        """Wait for a rule hook of the form "here_is_" + wmclass """
        # We don't need to read the second argument of the hook and don't need
        # to check that is indeed equals "here_is_" + wmclass. But we need to
        # check this once we create clients simultaneously.
        line = self.hc_idle.stdout.readline().rstrip('\n').split('\t')
        try:
            self.hc_idle.wait(0)
        except subprocess.TimeoutExpired:
            pass
        if self.hc_idle.returncode is not None:
            self.hlwm_process.investigate_timeout(
                'waiting for hook triggered by client \"{}\"'.format(wmclass))
        return line[-1]

    def shutdown(self):
        for client_proc in self.client_procs:
            client_proc.terminate()
            client_proc.wait(2)

        self.hc_idle.terminate()
        self.hc_idle.wait(2)

    def bool(self, python_bool_var):
        """convert a boolean variable into hlwm's string representation"""
        return "true" if python_bool_var else "false"


@pytest.fixture()
def hlwm(hlwm_process):
    display = os.environ['DISPLAY']
    # display = ':13'
    hlwm_bridge = HlwmBridge(display, hlwm_process)
    yield hlwm_bridge

    # Make sure that hlwm survived:
    hlwm_bridge.call('version')

    hlwm_bridge.shutdown()


class HlwmProcess:
    def __init__(self, tmpdir, env, args):
        autostart = tmpdir / 'herbstluftwm' / 'autostart'
        autostart.ensure()
        autostart.write(textwrap.dedent("""
            #!/usr/bin/env bash
            echo "hlwm started"
        """.lstrip('\n')))
        autostart.chmod(0o755)
        self.bin_path = os.path.join(BINDIR, 'herbstluftwm')
        self.proc = subprocess.Popen(
            [self.bin_path, '--verbose'] + args, env=env,
            bufsize=0,  # essential for reading output with selectors!
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        sel = selectors.DefaultSelector()
        sel.register(self.proc.stdout, selectors.EVENT_READ, data=sys.stdout)
        sel.register(self.proc.stderr, selectors.EVENT_READ, data=sys.stderr)
        self.output_selector = sel

        # Wait for marker output from wrapper script:
        self.read_and_echo_output(until_stdout='hlwm started')

    def read_and_echo_output(self, until_stdout=None, until_stderr=None, until_eof=False):
        expect_sth = ((until_stdout or until_stderr) is not None)
        max_wait = 5

        # Track which file objects have EOFed:
        eof_fileobjs = set()
        fileobjs = set(k.fileobj for k in self.output_selector.get_map().values())

        stderr = ''
        stdout = ''

        def match_found():
            if until_stdout and (until_stdout in stdout):
                return True
            if until_stderr and (until_stderr in stderr):
                return True
            return False

        started = datetime.now()
        while (datetime.now() - started).total_seconds() < max_wait:
            select_timeout = 1
            # If we're not polling for a matching string (anymore), there is no
            # need for a dampening timeout:
            if not expect_sth or match_found():
                select_timeout = 0
            selected = self.output_selector.select(timeout=select_timeout)
            for key, events in selected:
                # Read only single byte, otherwise we might block:
                ch = key.fileobj.read(1).decode('ascii')

                if ch == '':
                    eof_fileobjs.add(key.fileobj)

                # Pass it through to the real stdout/stderr:
                key.data.write(ch)
                key.data.flush()

                # Store in temporary buffer for string matching:
                if key.fileobj == self.proc.stderr:
                    stderr += ch
                if key.fileobj == self.proc.stdout:
                    stdout += ch

            if until_eof:
                # We are going to the very end, so carry on until all file
                # objects have returned EOF:
                if eof_fileobjs == fileobjs:
                    break
                else:
                    continue

            if selected != []:
                # There is still data available, so keep reading (no matter
                # what):
                continue

            # But stop reading if there is nothing to look for or we have
            # already found it:
            if not expect_sth or match_found():
                break

        duration = (datetime.now() - started).total_seconds()
        if expect_sth and not match_found():
            assert False, f'Expected string not encountered within {duration:.1f} seconds'

    @contextmanager
    def wait_stderr_match(self, match):
        """
        Context manager for wrapping commands that are expected to result in
        certain output on hlwm's stderr (e.g., input events).
        """
        self.read_and_echo_output()
        yield
        self.read_and_echo_output(until_stderr=match)

    def investigate_timeout(self, reason):
        """if some kind of client request observes a timeout, investigate the
        herbstluftwm server process. 'reason' is best phrased using present
        participle"""
        try:
            self.proc.wait(0)
        except subprocess.TimeoutExpired:
            pass
        if self.proc.returncode is None:
            raise Exception(str(reason) + " took too long"
                            + " but hlwm still running") from None
        else:
            raise Exception("{} made herbstluftwm quit with exit code {}"
                            .format(str(reason), self.proc.returncode)) from None

    def shutdown(self):
        self.proc.terminate()

        # Make sure to read and echo all remaining output (esp. ASAN messages):
        self.read_and_echo_output(until_eof=True)

        if self.proc.returncode is None:
            # only wait the process if it hasn't been cleaned up
            # this also avoids the second exception if hlwm crashed
            try:
                assert self.proc.wait(2) == 0
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(2)
                raise Exception("herbstluftwm did not quit on sigterm"
                                + " and had to be killed") from None


class HcIdle:
    def __init__(self, hlwm):
        self.hlwm = hlwm
        self.proc = subprocess.Popen([hlwm.HC_PATH, '--idle'],
                                     stdout=subprocess.PIPE,
                                     bufsize=0)
        # we don't know how fast self.proc connects to hlwm.
        # So we keep sending messages via hlwm util we receive some
        number_sent = 0
        while [] == select.select([self.proc.stdout], [], [], 0.1)[0]:
            # while there hasn't been a message received, send something
            number_sent += 1
            self.hlwm.call(['emit_hook', 'hc_idle_bootup', number_sent])
        # now that we know that self.proc is connected, we need to consume
        # its output up to the last message we have sent
        assert number_sent > 0
        number_received = 0
        while number_received < number_sent:
            line = self.proc.stdout.readline().decode().rstrip('\n').split('\t')
            assert line[0] == 'hc_idle_bootup'
            number_received = int(line[1])

    def hooks(self):
        """return all hooks since the last call of this function"""
        # collect all hooks so far, so collect all up to the following hook:
        self.hlwm.call(['emit_hook', 'hc_idle_logging_marker'])
        hooks = []
        while True:
            line = self.proc.stdout.readline().decode().rstrip('\n').split('\t')
            if line[0] == 'hc_idle_logging_marker':
                break
            else:
                hooks.append(line)
        return hooks

    def shutdown(self):
        self.proc.terminate()
        try:
            self.proc.wait(2)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait(2)


@pytest.fixture()
def hc_idle(hlwm):
    hc = HcIdle(hlwm)

    yield hc

    hc.shutdown()


def kill_all_existing_windows(show_warnings=True):
    xlsclients = subprocess.run(['xlsclients', '-l'],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                check=False)
    print(xlsclients.stderr.decode(), file=sys.stderr, end='')
    if re.search('unable to open display', xlsclients.stderr.decode()):
        # if the display is already closed since there are no clients left,
        # then that's fine for us
        return
    else:
        # otherwise, assert successfull termination
        assert xlsclients.returncode == 0
    clients = []
    for l in xlsclients.stdout.decode().splitlines():
        m = re.match(r'Window (0x[0-9a-fA-F]*):', l)
        if m:
            clients.append(m.group(1))
    if clients and show_warnings:
        warnings.warn(UserWarning("There are still some clients "
                                  "from previous tests."))
    for c in clients:
        if show_warnings:
            warnings.warn(UserWarning("Killing " + c))
        # send close and kill ungently
        subprocess.run(['xdotool', 'windowclose', c])
        subprocess.run(['xdotool', 'windowkill', c])


@pytest.fixture()
def hlwm_spawner(tmpdir):
    """yield a function to spawn hlwm"""
    assert os.environ['DISPLAY'] != ':0', 'Refusing to run tests on display that might be your actual X server (not Xvfb)'

    def spawn(args=[], display=None):
        if display is None:
            display = os.environ['DISPLAY']
        env = {
            'DISPLAY': display,
            'XDG_CONFIG_HOME': str(tmpdir),
        }
        return HlwmProcess(tmpdir, env, args)
    return spawn


@pytest.fixture()
def hlwm_process(hlwm_spawner):
    """Set up hlwm and also check that it shuts down gently afterwards"""
    hlwm_proc = hlwm_spawner(['--no-tag-import'])
    kill_all_existing_windows(show_warnings=True)

    yield hlwm_proc

    hlwm_proc.shutdown()
    kill_all_existing_windows(show_warnings=False)


@pytest.fixture(params=[0])
def running_clients(hlwm, running_clients_num):
    """
    Fixture that provides a number of already running clients, as defined by a
    "running_clients_num" test parameter.
    """
    return hlwm.create_clients(running_clients_num)


@pytest.fixture(scope="session")
def x11_connection():
    """ Long-lived fixture that maintains an open connection to the X11 display
    for the entire duration of all tests. This avoids issues caused by Xvfb and
    Xephyr resetting all properties whenever the last connection is closed. It
    is probably a bit more efficient, too. """
    display = None
    attempts_left = 10
    while display is None and attempts_left > 0:
        try:
            display = Xlib.display.Display()
            # the above call may result in an exception:
            # ConnectionResetError: [Errno 104] Connection reset by peer
            # However, the handling of this error in the above function results in a
            # type error, see
            # https://github.com/python-xlib/python-xlib/pull/160
        except TypeError as msg:
            # hence, just print the type error
            print("!!! TypeError: %s" % msg, file=sys.stderr)
            # wait for a moment, and then try again..
            time.sleep(2)
            attempts_left -= 1
        except Xlib.error.ConnectionClosedError as msg:
            print("!!! Xlib.error.ConnectionClosedError: %s" % msg, file=sys.stderr)
            # maybe the display was still shutting down
            # wait for a moment, and then try again..
            time.sleep(2)
            attempts_left -= 1
    yield display
    display.close()


@pytest.fixture()
def x11(x11_connection):
    """ Short-lived fixture for interacting with the X11 display and creating
    clients that are automatically destroyed at the end of each test. """
    class X11:
        def __init__(self, x11_connection):
            self.display = x11_connection
            self.windows = set()
            self.screen = self.display.screen()
            self.root = self.screen.root
            self.ewmh = ewmh.EWMH(self.display, self.root)
            self.hlwm = hlwm

        def window(self, winid_string):
            """return python-xlib window wrapper for a string window id"""
            winid_int = int(winid_string, 0)
            return self.display.create_resource_object('window', winid_int)

        def winid_str(self, window_handle):
            return hex(window_handle.id)

        def make_window_urgent(self, window):
            """make window urgent"""
            window.set_wm_hints(flags=Xutil.UrgencyHint)
            self.display.sync()

        def is_window_urgent(self, window):
            """check urgency of a given window handle"""
            hints = window.get_wm_hints()
            if hints is None:
                return False
            return bool(hints.flags & Xutil.UrgencyHint)

        def set_property_textlist(self, property_name, value, utf8=True, window=None):
            """set a ascii textlist property by its string name on the root window, or any other window"""
            if window is None:
                window = self.root
            prop = self.display.intern_atom(property_name)
            bvalue = bytearray()
            isfirst = True
            for entry in value:
                if isfirst:
                    isfirst = False
                else:
                    bvalue.append(0)
                bvalue += entry.encode()
            proptype = Xatom.STRING
            if utf8:
                proptype = self.display.get_atom('UTF8_STRING')
            window.change_property(prop, proptype, 8, bytes(bvalue))

        def set_property_cardinal(self, property_name, value, window=None):
            if window is None:
                window = self.root
            prop = self.display.intern_atom(property_name)
            window.change_property(prop, Xatom.CARDINAL, 32, value)

        def get_property(self, property_name, window=None):
            """get a property by its string name from the root window, or any other window"""
            if window is None:
                window = self.root
            prop = self.display.intern_atom(property_name)
            resp = window.get_full_property(prop, X.AnyPropertyType)
            return resp.value if resp is not None else None

        def create_client(self, urgent=False, pid=None,
                          geometry=(50, 50, 300, 200),  # x, y, width, height
                          force_unmanage=False,
                          sync_hlwm=True,
                          wm_class=None,
                          window_type=None,
                          transient_for=None,
                          ):
            w = self.root.create_window(
                geometry[0],
                geometry[1],
                geometry[2],
                geometry[3],
                2,
                self.screen.root_depth,
                X.InputOutput,
                X.CopyFromParent,
                background_pixel=self.screen.white_pixel,
                override_redirect=force_unmanage,
            )
            if wm_class is not None:
                w.set_wm_class(wm_class[0], wm_class[1])

            if transient_for is not None:
                w.set_wm_transient_for(transient_for)

            # Keep track of window for later removal:
            self.windows.add(w)

            w.set_wm_name('Some Window')
            if urgent:
                w.set_wm_hints(flags=Xutil.UrgencyHint)

            if window_type is not None:
                w.change_property(self.display.intern_atom('_NET_WM_WINDOW_TYPE'),
                                  Xatom.ATOM,
                                  32,
                                  [self.display.intern_atom(window_type)])

            if pid is not None:
                w.change_property(self.display.intern_atom('_NET_WM_PID'),
                                  Xatom.CARDINAL,
                                  32,
                                  [pid])

            w.map()
            self.display.sync()
            if sync_hlwm:
                # wait for hlwm to fully recognize it as a client
                self.sync_with_hlwm()
            return w, self.winid_str(w)

        def sync_with_hlwm(self):
            # wait for hlwm to flush all events:
            hlwm_bridge = HlwmBridge.INSTANCE
            assert hlwm_bridge is not None, "hlwm must be running"
            hlwm_bridge.call('true')

        def get_absolute_top_left(self, window):
            """return the absolute (x,y) coordinate of the given window,
            i.e. relative to the root window"""
            x = 0
            y = 0
            while True:
                # the following coordinates are only relative
                # to the parent of window
                geom = window.get_geometry()
                print('Geometry of {} is: x={} y={} w={} h={}'.format(
                      self.winid_str(window), geom.x, geom.y, geom.width, geom.height))
                x += geom.x
                y += geom.y
                # check if the window's parent is already the root window
                tree = window.query_tree()
                if tree.root == tree.parent:
                    break
                # if it's not, continue at its parent
                window = tree.parent
            return (x, y)

        def get_hlwm_frames(self):
            """get list of window handles of herbstluftwm
            frame decoration windows"""
            cmd = ['xdotool', 'search', '--class', '_HERBST_FRAME']
            frame_wins = subprocess.run(cmd,
                                        stdout=subprocess.PIPE,
                                        universal_newlines=True,
                                        check=True)
            res = []
            for winid_decimal_str in frame_wins.stdout.splitlines():
                res.append(self.window(winid_decimal_str))
            return res

        def shutdown(self):
            # Destroy all created windows:
            for window in self.windows:
                window.unmap()
                window.destroy()
            self.display.sync()

    x11_ = X11(x11_connection)
    yield x11_
    x11_.shutdown()


class MultiscreenDisplay:
    """context manager for creating a server display with multiple
    screens. We support two xserver programs: Xephyr and Xvfb:

      - Xephyr places screens side by side. Since Xephyr opens
        one window per screen itself, this has to run inside an existing Xvfb.
      - Xvfb makes all screens have the coordinate (0, 0)
    """
    def __init__(self, server='Xephyr', screens=[(800, 600)], extra_args=[]):
        """ Creates a new x server (where 'server' is 'Xephyr' or 'Xvfb')
        that has all the specified screens.
        """
        # we build up the full command tu run step by step:
        self.full_cmd = [server, '-nolisten', 'tcp']

        # pass the -screen parameters
        # ---------------------------
        self.screen_rects = []
        current_x_offset = 0
        for idx, (width, height) in enumerate(screens):
            self.screen_rects.append([current_x_offset, 0, width, height])
            geo = '{}x{}x8'.format(width, height)
            if server == 'Xvfb':
                self.full_cmd += ['-screen', str(idx), geo]
            else:
                self.full_cmd += ['-screen', geo]
                # xephyr places them side by side:
                current_x_offset += width
        self.full_cmd += extra_args

        # let the xserver find a display and write its name to a pipe
        # -----------------------------------------------------------
        pipe_read, pipe_write = os.pipe()
        self.full_cmd += ['-displayfd', str(pipe_write)]

        print("Running: {}".format(' '.join(self.full_cmd)))
        self.proc = subprocess.Popen(self.full_cmd, pass_fds=[pipe_write])

        # read the display number from the pipe
        # -------------------------------------
        display_bytes = bytes()
        # read bytes until the newline
        while True:
            chunk = os.read(pipe_read, 1)
            display_bytes += chunk
            if len(chunk) < 1 or chunk == b'\n':
                break
        os.close(pipe_read)
        self.display = ':' + display_bytes.decode().rstrip()
        print(server + " is using the display \"{}\"".format(self.display))

    def screens(self):
        """return a list of screen geometries, where a geometry
        is a list [x, y, width, height]
        """
        return self.screen_rects

    def __enter__(self):
        return self

    def __exit__(self, type_param, value, traceback):
        self.proc.terminate()
        self.proc.wait(5)


@pytest.fixture()
def keyboard():
    class KeyBoard:
        def press(self, key_spec):
            subprocess.check_call(['xdotool', 'key', key_spec])

        def down(self, key_spec):
            subprocess.check_call(['xdotool', 'keydown', key_spec])

        def up(self, key_spec):
            subprocess.check_call(['xdotool', 'keyup', key_spec])

    return KeyBoard()


@pytest.fixture()
def mouse(hlwm_process):
    class Mouse:
        def move_into(self, win_id, x=1, y=1):
            self.call_cmd(f'xdotool mousemove --sync --window {win_id} {x} {y}', shell=True)

        def click(self, button, into_win_id=None, wait=True):
            if into_win_id:
                self.move_into(into_win_id)
            if wait:
                with hlwm_process.wait_stderr_match('ButtonPress'):
                    subprocess.check_call(['xdotool', 'click', button])
            else:
                subprocess.check_call(['xdotool', 'click', button])

        def move_to(self, abs_x, abs_y):
            abs_x = str(int(abs_x))
            abs_y = str(int(abs_y))
            self.call_cmd(f'xdotool mousemove --sync {abs_x} {abs_y}', shell=True)

        def move_relative(self, delta_x, delta_y):
            self.call_cmd(f'xdotool mousemove_relative --sync {delta_x} {delta_y}', shell=True)

        def call_cmd(self, cmd, shell=False):
            print('calling: {}'.format(cmd), file=sys.stderr)
            subprocess.check_call(cmd, shell=shell)

    return Mouse()
