#!/usr/bin/env python3
import shlex
import subprocess
from herbstluftwm.types import HlwmType
from typing import List
import struct
"""
Python bindings for herbstluftwm. The central entity for communication
with the herbstluftwm server is the Herbstluftwm class. See the example.py
for example usages of the classes provided here.

   import herbstluftwm

   hlwm = herbstluftwm.Herbstluftwm()
   hlwm.call('add new_tag')
"""


class Herbstluftwm:
    """A herbstluftwm wrapper class that
    gives access to the remote interface of herbstluftwm.

    Example:

        print(Herbstluftwm().call('add new_tag').stdout)
        print(Herbstluftwm().call(['add', 'new_tag']).stdout)
    """

    def __init__(self, herbstclient='herbstclient'):
        """
        Create a wrapper object. The herbstclient parameter is
        the path or command to the 'herbstclient' executable.
        """
        self.herbstclient_path = herbstclient
        self.env = None
        self.proc = None

    def open_persistent_pipe(self):
        """
        Establish a persistent pipe
        """
        self.proc = subprocess.Popen([self.herbstclient_path, '--binary-pipe'],
                                     stdout=subprocess.PIPE,
                                     stdin=subprocess.PIPE,
                                     env=self.env,
                                     encoding=None,  # open stdout/stdin in binary mode
                                     )

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

    @staticmethod
    def _read_text_until_null_byte(stream):
        buf = bytearray()
        while True:
            byte = stream.read(1)
            if len(byte) == 0:
                return None
            if byte[0] == 0:
                break
            buf += byte
        return buf.decode()

    def unchecked_call(self, cmd):
        """Call the command but do not check exit code or stderr"""
        args = self._parse_command(cmd)

        if self.proc is not None:
            nullbyte = struct.pack('B', 0)
            for arg in args:
                self.proc.stdin.write(b'ARG')
                self.proc.stdin.write(nullbyte)
                self.proc.stdin.write(arg.encode())
                self.proc.stdin.write(nullbyte)
            self.proc.stdin.write(b'RUN')
            self.proc.stdin.write(nullbyte)
            self.proc.stdin.flush()
            reply = {
                'STDOUT': '',
                'STDERR': '',
                'STATUS': '',
            }
            for _ in range(0, 3):
                fieldname = Herbstluftwm._read_text_until_null_byte(self.proc.stdout)
                if fieldname is None:
                    raise Exception('herbstclient did non print a full reply')
                fieldvalue = Herbstluftwm._read_text_until_null_byte(self.proc.stdout)
                if fieldvalue is None:
                    raise Exception('herbstclient did non print a full reply')
                reply[fieldname] = fieldvalue
            #
            complete_proc = subprocess.CompletedProcess(args, int(reply['STATUS']))
            complete_proc.stdout = reply['STDOUT']
            complete_proc.stderr = reply['STDERR']
        else:
            complete_proc = subprocess.run(
                [self.herbstclient_path, '-n'] + args,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                env=self.env,
                universal_newlines=True,
                # Kill hc when it hangs due to crashed server:
                timeout=2)

        return complete_proc

    def call(self, cmd, allowed_stderr=None):
        """call the command and expect it to have exit code zero
        and no output on stderr.
        if allowed_stderr is set, then it should be a regex object
        (something with .match) that matches every line the command
        writes to stderr.
        """
        proc = self.unchecked_call(cmd)
        assert proc.returncode == 0
        if allowed_stderr is None:
            assert not proc.stderr
        else:
            for line in proc.stderr.splitlines():
                assert allowed_stderr.match(line)
        return proc

    @property
    def attr(self) -> 'AttributeProxy':
        """return an attribute proxy"""
        return AttributeProxy(self, [])


class AttributeProxy:
    """
    An AttributeProxy object represents an object or attribute in herbstluftwm's
    object tree. A proxy object can be used to

      - query attribute values
      - set attribute values
      - create custom attributes
      - access children
    """
    _herbstluftwm: Herbstluftwm  # the herbstclient wrapper
    _path: List[str]  # the path of this attribute/object

    def __init__(self, herbstluftwm: Herbstluftwm, attribute_path):
        self._herbstluftwm = herbstluftwm
        self._path = attribute_path

    def __repr__(self) -> str:
        path = AttributeProxy._compose_path(self._path)
        return f'<AttributeProxy {path}>'

    @staticmethod
    def _compose_path(path: List[str]) -> str:
        return '.'.join(map(str, path))

    def _get_value_from_hlwm(self):
        attr_path = AttributeProxy._compose_path(self._path)
        command = chain('and', [
            ['attr_type', attr_path],
            ['get_attr', attr_path]
        ])
        lines = self._herbstluftwm.call(command).stdout.split('\n', maxsplit=1)
        if len(lines) == 2:
            type_name, value = lines
        elif len(lines) == 1:
            type_name = lines[0]
            value = ''
        else:
            type_name = ''
            value = ''
        hlwm_type = HlwmType.by_name(type_name)
        if hlwm_type is None:
            # just return the string
            return value
        else:
            return hlwm_type.from_user_str(value)

    def __call__(self) -> str:
        return self._get_value_from_hlwm()

    def __str__(self) -> str:
        """
        get plain string representation of the attribute value
        """
        command = ['get_attr', AttributeProxy._compose_path(self._path)]
        return self._herbstluftwm.call(command).stdout

    def __getattr__(self, name) -> 'AttributeProxy':
        if str(name).startswith('_'):
            return super(AttributeProxy, self).__getattr__(name)
        else:
            return AttributeProxy(self._herbstluftwm, self._path + [name])

    def __getitem__(self, name):
        """alternate syntax for attributes/children
        containing a - or starting with a digit
        """
        return self.__getattr__(name)

    def __setattr__(self, name, value) -> None:
        if name[0] == '_':
            super(AttributeProxy, self).__setattr__(name, value)
        else:
            attr_path = AttributeProxy._compose_path(self._path + [name])
            hlwm_type = HlwmType.by_type_of_variable(value)
            command = [
                'set_attr',
                attr_path,
                hlwm_type.to_user_str(value) if hlwm_type is not None else str(value),
            ]
            if str(name).startswith('my_'):
                # for custom attributes, silently ensure that the attribute
                # exists.
                if hlwm_type is None:
                    type_name = 'string'
                else:
                    type_name = hlwm_type.name
                command = chain('chain', [
                    ['try', 'silent', 'new_attr', type_name, attr_path],
                    command
                ])
            self._herbstluftwm.call(command)

    def __setitem__(self, name, value) -> None:
        """alternate syntax for attributes/children
        containing a - or starting with a digit
        """
        self.__setattr__(name, value)


def chain(chain_cmd, commands: List[List[str]]) -> List[str]:
    """return a composed command that executes
    the commands given in the list. chain_cmd is one of:
      - chain
      - and
      - or
    """
    def separator_clashes(separator_name):
        for cmd in commands:
            if separator_name in cmd:
                return True
        return False
    # find a token that does not occur in any of the commands
    separator_num = 0
    while separator_clashes(f'S{separator_num}'):
        separator_num += 1
    separator = f'S{separator_num}'
    # create the composed command using the separator
    full_command = [chain_cmd]
    for cmd in commands:
        full_command += [separator] + cmd
    return full_command
