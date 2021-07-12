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
import herbstluftwm

import pytest


BINDIR = os.path.join(os.path.abspath(os.environ['PWD']))

# List of environment variables copied during hlwm process creation:
# * LSAN_OPTIONS: needed to suppress warnings about known memory leaks
COPY_ENV_WHITELIST = ['LSAN_OPTIONS']

# time in seconds to wait for a process to shut down
PROCESS_SHUTDOWN_TIME = 30


def extend_env_with_whitelist(environment):
    """Copy some whitelisted environment variables (if set) into an existing environment"""
    environment.update({e: os.environ[e] for e in os.environ if e in COPY_ENV_WHITELIST})
    return environment


class HlwmBridge(herbstluftwm.Herbstluftwm):

    HC_PATH = os.path.join(BINDIR, 'herbstclient')
    # if there is some HlwmBridge, then it is registered here:
    INSTANCE = None

    def __init__(self, display, hlwm_process):
        herbstluftwm.Herbstluftwm.__init__(self, herbstclient=self.HC_PATH)
        HlwmBridge.INSTANCE = self
        self.client_procs = []
        self.next_client_id = 0
        self.env = {
            'DISPLAY': display,
        }
        self.env = extend_env_with_whitelist(self.env)
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

    def unchecked_call(self, cmd, log_output=True, read_hlwm_output=True):
        """call the command but do not check exit code or stderr"""
        args = self._parse_command(cmd)

        try:
            proc = herbstluftwm.Herbstluftwm.unchecked_call(self, cmd)
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

    def call_xfail(self, cmd):
        """ Call the command, expect it to terminate with a non-zero exit code,
        emit no output on stdout but some output on stderr. The returned
        process handle offers a match() method that checks the stderr against a
        given regex. """
        proc = self.unchecked_call(cmd)
        assert proc.returncode != 0
        assert proc.stdout == ""
        assert proc.stderr != ""

        def f(self2, needle, regex=True):
            if regex:
                assert re.search(needle, self2.stderr)
            else:
                assert self2.stderr.find(needle)
            # allow to list multiple 'expect_stderr()' statements:
            return self2

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
        should be evaluated. If this is set, one cannot distinguish between
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
            old_items = items
            items = []
            for s in old_items:
                unescaped = shlex.split(s)
                items.append(unescaped[0] if len(unescaped) else '')
        return sorted(items)

    def command_has_all_args(self, cmd):
        """
        ask the completion and assert that the given command does not allow
        any further arguments
        """
        res = self.unchecked_call(['complete_shell', len(cmd)] + cmd)
        assert res.returncode == 7
        assert res.stdout == ''
        assert res.stderr == ''

    def list_children_via_attr(self, object_path):
        """
        List the names of children of the
        given object, using the attr command internally.
        """
        # regexes for list_children:

        children_re = \
            re.compile(r'[0-9]* (child|children)[\\.:]((\n  [^\n]*)*)')
        line_re = re.compile('^  (.*)\\.$')
        output = self.call(['attr', object_path]).stdout
        section_match = children_re.search(output)
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
        # first send SIGTERM to all processes, so they
        # can shut down in parallel
        for client_proc in self.client_procs:
            client_proc.terminate()
        self.hc_idle.terminate()

        # and then wait for each of them to finish:
        for client_proc in self.client_procs:
            client_proc.wait(PROCESS_SHUTDOWN_TIME)
        self.hc_idle.wait(PROCESS_SHUTDOWN_TIME)
        self.hc_idle.stdout.close()

    def bool(self, python_bool_var):
        """convert a boolean variable into hlwm's string representation"""
        return "true" if python_bool_var else "false"


@pytest.fixture()
def hlwm(hlwm_process, xvfb):
    # display = ':13'
    hlwm_bridge = HlwmBridge(xvfb.display, hlwm_process)
    yield hlwm_bridge

    # Make sure that hlwm survived:
    hlwm_bridge.call('version')

    hlwm_bridge.shutdown()


class HlwmProcess:
    def __init__(self, autostart_stdout_message, env, args):
        """create a new hlwm process and wait for booting up.
        - autostart_stdout_message is the message printed to stdout
          that indicates that the autostart has been executed entirely
        - env is the environment dictionary
        - args is a list of additional command line arguments
        """
        self.bin_path = os.path.join(BINDIR, 'herbstluftwm')
        self.proc = subprocess.Popen(
            [self.bin_path, '--exit-on-xerror', '--verbose'] + args, env=env,
            bufsize=0,  # essential for reading output with selectors!
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        sel = selectors.DefaultSelector()
        sel.register(self.proc.stdout, selectors.EVENT_READ, data=sys.stdout)
        sel.register(self.proc.stderr, selectors.EVENT_READ, data=sys.stderr)
        self.output_selector = sel

        # Wait for marker output from wrapper script:
        self.read_and_echo_output(until_stdout=autostart_stdout_message)

    def read_and_echo_output(self, until_stdout=None, until_stderr=None, until_eof=False):
        expect_sth = ((until_stdout or until_stderr) is not None)
        max_wait = 15

        # Track which file objects have EOFed:
        eof_fileobjs = set()
        fileobjs = set(k.fileobj for k in self.output_selector.get_map().values())

        stderr = ''
        stderr_bytes = bytes()
        stdout = ''
        stdout_bytes = bytes()

        def match_found():
            if until_stdout:
                if isinstance(until_stdout, str) and until_stdout in stdout:
                    return True
                if hasattr(until_stdout, 'search') and until_stdout.search(stdout):
                    return True
            if until_stderr:
                if isinstance(until_stderr, str) and until_stderr in stderr:
                    return True
                if hasattr(until_stderr, 'search') and until_stderr.search(stderr):
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
                ch = key.fileobj.read(1)

                if ch == b'':
                    eof_fileobjs.add(key.fileobj)

                # Store in temporary buffer for string matching:
                if key.fileobj == self.proc.stderr:
                    stderr_bytes += ch
                    if ch == b'\n':
                        stderr += stderr_bytes.decode()
                        # Pass it through to the real stderr:
                        key.data.write(stderr_bytes.decode())
                        key.data.flush()
                        stderr_bytes = b''

                if key.fileobj == self.proc.stdout:
                    stdout_bytes += ch
                    if ch == b'\n':
                        stdout += stdout_bytes.decode()
                        # Pass it through to the real stdout:
                        key.data.write(stdout_bytes.decode())
                        key.data.flush()
                        stdout_bytes = b''

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

        # decode remaining bytes for the final match_found() check
        if stderr_bytes != b'':
            stderr += stderr_bytes.decode()
            sys.stderr.write(stderr_bytes.decode())
            sys.stderr.flush()
        if stdout_bytes != b'':
            stdout += stdout_bytes.decode()
            sys.stdout.write(stdout_bytes.decode())
            sys.stdout.flush()
        duration = (datetime.now() - started).total_seconds()
        if expect_sth and not match_found():
            assert False, f'Expected string not encountered within {duration:.1f} seconds'

    @contextmanager
    def wait_stdout_match(self, match):
        """
        Context manager for wrapping commands that are expected to result in
        certain output on hlwm's stdout (e.g., input events).

        Warning: do not run call(...) within such a context, but only
        unchecked_call(..., read_hlwm_output=False) instead
        """
        self.read_and_echo_output()
        yield
        self.read_and_echo_output(until_stdout=match)

    @contextmanager
    def wait_stderr_match(self, match):
        """
        Context manager for wrapping commands that are expected to result in
        certain output on hlwm's stderr (e.g., input events).

        Warning: do not run call(...) within such a context, but only
        unchecked_call(..., read_hlwm_output=False) instead
        """
        self.read_and_echo_output()
        yield
        self.read_and_echo_output(until_stderr=match)

    def investigate_timeout(self, reason):
        """if some kind of client request observes a timeout, investigate the
        herbstluftwm server process. 'reason' is best phrased using present
        participle"""
        self.read_and_echo_output()
        try:
            self.proc.wait(0)
        except subprocess.TimeoutExpired:
            pass
        self.read_and_echo_output()
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
        self.proc.stdout.close()
        self.proc.stderr.close()

        if self.proc.returncode is None:
            # only wait the process if it hasn't been cleaned up
            # this also avoids the second exception if hlwm crashed
            try:
                assert self.proc.wait(PROCESS_SHUTDOWN_TIME) == 0
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(PROCESS_SHUTDOWN_TIME)
                raise Exception("herbstluftwm did not quit on sigterm"
                                + " and had to be killed") from None


class HcIdle:
    def __init__(self, hlwm, zero_separated=False):
        """
        hlwm is the HlwmBridge
        zero_separated denotes whether the hooks are zero byte separated
        (otherwise, they are newline-separated)
        """
        self.hlwm = hlwm
        self.separator = b'\n'
        command = [hlwm.HC_PATH, '--idle']
        if zero_separated:
            command = [hlwm.HC_PATH, '-0', '--idle']
            self.separator = b'\x00'
        self.proc = subprocess.Popen(command,
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
            line = self.read_hook()
            assert line[0] == 'hc_idle_bootup'
            number_received = int(line[1])

    def read_hook(self):
        """read exactly one hook. This blocks if there is on herbstclient's stdout none."""
        line_bytes = b''
        while True:
            b = self.proc.stdout.read(1)
            if b == self.separator:
                break
            else:
                line_bytes += b
        return line_bytes.decode().split('\t')

    def hooks(self):
        """return all hooks since the last call of this function"""
        # collect all hooks so far, so collect all up to the following hook:
        self.hlwm.call(['emit_hook', 'hc_idle_logging_marker'])
        hooks = []
        while True:
            line = self.read_hook()
            if line[0] == 'hc_idle_logging_marker':
                break
            else:
                hooks.append(line)
        return hooks

    def shutdown(self):
        self.proc.terminate()
        try:
            self.proc.wait(PROCESS_SHUTDOWN_TIME)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait(PROCESS_SHUTDOWN_TIME)
        self.proc.stdout.close()


@pytest.fixture()
def hc_idle(hlwm):
    hc = HcIdle(hlwm)

    yield hc

    hc.shutdown()


@pytest.fixture()
def hlwm_spawner(tmpdir):
    """yield a function to spawn hlwm"""
    assert xvfb is not None, 'Refusing to run tests in a non-Xvfb environment (possibly your actual X server?)'

    def spawn(args=[], display=None):
        if display is None:
            display = os.environ['DISPLAY']
        env = {
            'DISPLAY': display,
            'XDG_CONFIG_HOME': str(tmpdir),
        }
        env = extend_env_with_whitelist(env)
        autostart = tmpdir / 'herbstluftwm' / 'autostart'
        autostart.ensure()
        autostart.write(textwrap.dedent("""
            #!/usr/bin/env bash
            echo "hlwm started"
        """.lstrip('\n')))
        autostart.chmod(0o755)
        return HlwmProcess('hlwm started', env, args)
    return spawn


@pytest.fixture()
def xvfb(request):
    # start an Xvfb server (don't start Xephyr because
    # Xephyr requires another runnig xserver already).
    # also we add '-noreset' such that the server is not reset
    # when the last client connection is closed.
    #
    # the optional parameter is a dict for different settings. If you want
    # another resolution, then annotate the test case with:
    #
    #    @pytest.mark.parametrize("xvfb", [{'resolution': (1280, 1024)}], indirect=True)
    #
    parameters = {
        'screens': [(800, 600)],
        'xrender': True,
    }
    if hasattr(request, 'param'):
        parameters.update(request.param)
    if 'resolution' in parameters:
        parameters['screens'] = [parameters['resolution']]
    args = ['-noreset']
    if parameters['xrender']:
        args += ['+extension', 'RENDER']
    with MultiscreenDisplay(server='Xvfb', screens=parameters['screens'], extra_args=args) as xserver:
        os.environ['DISPLAY'] = xserver.display
        yield xserver


@pytest.fixture()
def hlwm_process(hlwm_spawner, request, xvfb):
    parameters = {
        'transparency': True,
    }
    if hasattr(request, 'param'):
        # the param must contain a dictionary possibly
        # updating the default parameters
        parameters.update(request.param)

    """Set up hlwm and also check that it shuts down gently afterwards"""
    additional_commandline = ['--no-tag-import']
    if not parameters['transparency']:
        additional_commandline += ['--no-transparency']
    hlwm_proc = hlwm_spawner(additional_commandline, display=xvfb.display)

    yield hlwm_proc

    hlwm_proc.shutdown()


@pytest.fixture(params=[0])
def running_clients(hlwm, running_clients_num):
    """
    Fixture that provides a number of already running clients, as defined by a
    "running_clients_num" test parameter.
    """
    return hlwm.create_clients(running_clients_num)


@pytest.fixture()
def x11_connection(xvfb):
    """Connect to the given xvfb display and return a Xlib.display handle"""
    display = xlib_connect_to_display(xvfb.display)
    yield display
    display.close()


def xlib_connect_to_display(display):
    """try to connect to a display using Xlib.display.Display
    and work around a bug in python-xlib.
    The resuling display object still needs to be closed on shut down.
    """
    display = None
    attempts_left = 10
    while display is None and attempts_left > 0:
        try:
            display = Xlib.display.Display(display)
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
    return display


class RawImage:
    def __init__(self, data, width, height):
        """
        A raw image is an array of size width*height containing
        (r, g, b) triples, line by line
        """
        self.data = data
        self.width = width
        self.height = height
        assert len(data) == width * height

    def pixel(self, x, y):
        """return (r, g, b) triple for a pixel"""
        return self.data[x + self.width * y]

    def color_count(self, rgb_triple):
        count = 0
        for pix in self.data:
            if pix == rgb_triple:
                count += 1
        return count

    @staticmethod
    def rgb2string(rgb_triple):
        return '#%02x%02x%02x' % rgb_triple


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

    def set_property_textlist(self, property_name, value, utf8=True, property_type=None, window=None):
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
        if property_type is None:
            property_type = Xatom.STRING
            if utf8:
                property_type = self.display.get_atom('UTF8_STRING')
        window.change_property(prop, property_type, 8, bytes(bvalue))

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
                      pre_map=lambda wh: None,  # called before the window is mapped
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

        pre_map(w)
        w.map()
        self.display.sync()
        if sync_hlwm:
            # wait for hlwm to fully recognize it as a client
            self.sync_with_hlwm()
        return w, self.winid_str(w)

    def screenshot(self, win_handle) -> RawImage:
        """screenshot of a windows content, not including its border"""
        geom = win_handle.get_geometry()
        attr = win_handle.get_attributes()
        # Xlib defines AllPlanes as: ((unsigned long)~0L)
        all_planes = 0xffffffff
        # Maybe, the following get_image() works differently
        # than XGetImage(), because we still need to interpret
        # the pixel values using the colormap, whereas the man
        # page for XGetImage() does not mention 'colormap' at all
        raw = win_handle.get_image(0, 0, geom.width, geom.height,
                                   X.ZPixmap, all_planes)
        # raw.data is a array of pixel-values, which need to
        # be interpreted using the colormap:
        if raw.depth == 8:
            colorDict = {}
            for pixel in raw.data:
                colorDict[pixel] = None
            colorPixelList = list(colorDict)
            colorRGBList = attr.colormap.query_colors(colorPixelList)
            for pixelval, rgbval in zip(colorPixelList, colorRGBList):
                # Useful debug output if something blows up again (which is likely)
                # print(f'{pixelval} -> {rgbval.red}  {rgbval.green} {rgbval.blue}')
                colorDict[pixelval] = (rgbval.red % 256, rgbval.green % 256, rgbval.blue % 256)
            # the image size is enlarged such that the width
            # is a multiple of 4. Hence we remove these extra
            # columns in the end when mapping colorDict[] over the data array
            width_padded = geom.width
            while width_padded % 4 != 0:
                width_padded += 1
            rgbvals = [colorDict[p] for idx, p in enumerate(raw.data) if idx % width_padded < geom.width]
        else:
            assert raw.depth in [32, 24]
            # both for depth 32 and 24, the order is BGRA
            pixelsize = 4
            (blue, green, red) = (0, 1, 2)
            size = geom.width * geom.height
            assert len(raw.data) == pixelsize * size
            rgbvals = [(raw.data[pixelsize * (y * geom.width + x) + red],
                        raw.data[pixelsize * (y * geom.width + x) + green],
                        raw.data[pixelsize * (y * geom.width + x) + blue])
                       for y in range(0, geom.height)
                       for x in range(0, geom.width)]
        return RawImage(rgbvals, geom.width, geom.height)

    def decoration_screenshot(self, win_handle):
        decoration = self.get_decoration_window(win_handle)
        return self.screenshot(decoration)

    def sync_with_hlwm(self):
        self.display.sync()
        # wait for hlwm to flush all events:
        hlwm_bridge = HlwmBridge.INSTANCE
        assert hlwm_bridge is not None, "hlwm must be running"
        hlwm_bridge.call('true')

    def get_decoration_window(self, window):
        tree = window.query_tree()
        if tree.root == tree.parent:
            return None
        else:
            return tree.parent

    def get_absolute_top_left(self, window):
        """return the absolute (x,y) coordinate of the given window,
        i.e. relative to the root window"""
        x = 0
        y = 0
        while True:
            # the following coordinates are only relative
            # to the parent of window
            geom = window.get_geometry()
            print('Relative geometry of {} is: x={} y={} w={} h={}'.format(
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

    def get_absolute_geometry(self, window):
        """return the geometry of the window, where the top left
        coordinate comes from get_absolute_top_left()
        """
        x, y = self.get_absolute_top_left(window)
        geom = window.get_geometry()
        geom.x = x
        geom.y = y
        return geom

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


@pytest.fixture()
def x11(x11_connection):
    """ Short-lived fixture for interacting with the X11 display and creating
    clients that are automatically destroyed at the end of each test. """

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
        depth = 24
        for idx, (width, height) in enumerate(screens):
            self.screen_rects.append([current_x_offset, 0, width, height])
            geo = '{}x{}x{}'.format(width, height, depth)
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
        os.close(pipe_write)
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
        self.proc.wait(PROCESS_SHUTDOWN_TIME)


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
def mouse(hlwm_process, hlwm):
    class Mouse:
        def __init__(self):
            self.move_to(0, 0, wait=True)

        def move_into(self, win_id, x=1, y=1, wait=True):
            if wait:
                with hlwm_process.wait_stderr_match('EnterNotify'):
                    # no --sync here, because we're waiting for the EnterNotify anyways
                    self.call_cmd(f'xdotool mousemove --window {win_id} {x} {y}', shell=True)
                # reaching this line only means that hlwm started processing
                # the EnterNotify. So we need to wait until the event is fully processed:
                hlwm.call('true')
            else:
                self.call_cmd(f'xdotool mousemove --sync --window {win_id} {x} {y}', shell=True)

        def click(self, button, into_win_id=None, wait=True):
            if into_win_id:
                # no need to wait for herbstluftwm to process, we're just positioning the mouse to click
                self.move_into(into_win_id, wait=False)
            if wait:
                with hlwm_process.wait_stderr_match('ButtonPress'):
                    subprocess.check_call(['xdotool', 'click', button])
                # reaching this line only means that hlwm started processing
                # the ButtonPress. So we need to wait until the event is fully processed:
                hlwm.call('true')
            else:
                subprocess.check_call(['xdotool', 'click', button])

        def move_to(self, abs_x, abs_y, wait=True):
            abs_x = str(int(abs_x))
            abs_y = str(int(abs_y))
            self.call_cmd(f'xdotool mousemove --sync {abs_x} {abs_y}', shell=True)
            if wait:
                # wait until all the mouse move events that are now in the queue
                # are fully processed:
                hlwm.call('true')

        def move_relative(self, delta_x, delta_y, wait=True):
            self.call_cmd(f'xdotool mousemove_relative --sync {delta_x} {delta_y}', shell=True)
            if wait:
                # wait until all the mouse move events that were put in the
                # queue by the above xdotool invokation are fully processed.
                # (other than for the button events, the motion notify events
                # are not printed to stderr, because this would lead to
                # to much debug output on motions of the physical mouse)
                hlwm.call('true')

        def mouse_press(self, button, wait=True):
            cmd = ['xdotool', 'mousedown', button]
            if wait:
                with hlwm_process.wait_stderr_match('ButtonPress'):
                    subprocess.check_call(cmd)
                # wait for the ButtonPress to be fully processed:
                hlwm.call('true')
            else:
                subprocess.check_call(cmd)

        def mouse_release(self, button, wait=True):
            cmd = ['xdotool', 'mouseup', button]
            if wait:
                with hlwm_process.wait_stderr_match('ButtonRelease'):
                    subprocess.check_call(cmd)
                # wait for the ButtonRelease to be processed
                hlwm.call('true')
            else:
                subprocess.check_call(cmd)

        def call_cmd(self, cmd, shell=False):
            print('calling: {}'.format(cmd), file=sys.stderr)
            subprocess.check_call(cmd, shell=shell)

    return Mouse()
