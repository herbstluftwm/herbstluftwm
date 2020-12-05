#!/usr/bin/env python3
import shlex
import subprocess


class Herbstluftwm:
    """A herbstluftwm wrapper class that
    gives access to the remote interface of herbstluftwm.
    """

    def __init__(self, herbstclient='herbstclient'):
        """
        Create a wrapper object. The herbstclient parameter is
        the path or command to the 'herbstclient' executable.
        """
        self.herbstclient_path = herbstclient
        self.env = None

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

    def unchecked_call(self, cmd):
        """Call the command but do not check exit code or stderr"""
        args = self._parse_command(cmd)

        proc = subprocess.run([self.herbstclient_path, '-n'] + args,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              env=self.env,
                              universal_newlines=True,
                              # Kill hc when it hangs due to crashed server:
                              timeout=2
                              )

        return proc

    def call(self, cmd):
        """call the command and expect it to have exit code zero
        and no output on stderr"""
        proc = self.unchecked_call(cmd)
        assert proc.returncode == 0
        assert not proc.stderr
        return proc
