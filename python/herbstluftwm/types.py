import re
"""
Module containing types used in the communication with herbstluftwm;
primarily, types used in attributes.
"""


class Rectangle:
    """
    A rectangle on the screen, defined by its size and its distance to
    the top left screen corner.
    """
    def __init__(self, x=0, y=0, width=0, height=0):
        self.x = x
        self.y = y
        self.width = width
        self.height = height

    def __str__(self):
        return f'Rectangle({self.x}, {self.y}, {self.width}, {self.height})'

    def __repr__(self):
        return f'Rectangle({self.x}, {self.y}, {self.width}, {self.height})'

    def __eq__(self, other):
        return self.x == other.x \
            and self.y == other.y \
            and self.width == other.width \
            and self.height == other.height

    def to_user_str(self):
        return "%dx%d%+d%+d" % (self.width, self.height, self.x, self.y)

    @staticmethod
    def from_user_str(string):
        reg = '([0-9]+)x([0-9]+)([+-][0-9]+)([+-][0-9]+)'
        m = re.match(reg, string)
        if m is None:
            raise Exception(f'"{string}" is not in format {reg}')
        w = int(m.group(1))
        h = int(m.group(2))
        x = int(m.group(3))
        y = int(m.group(4))
        return Rectangle(x, y, w, h)

    def adjusted(self, dx=0, dy=0, dw=0, dh=0):
        """return a new rectangle whose components
        are adjusted by the provided deltas.
        """
        return Rectangle(self.x + dx, self.y + dy, self.width + dw, self.height + dh)


class HlwmType:
    """
    Wrapper functions for converting between python types and types
    in herbstluftwm.
    """
    def __init__(self, name, from_user_str, to_user_str, is_instance):
        # a hlwm type should define the following
        self.name = name  # type: str
        # a callback for parsing
        self.from_user_str = from_user_str  # of type: str -> T
        # a callback for printing
        self.to_user_str = to_user_str  # of type: T -> str
        # a predicate whether a python variable has this type:
        self.is_instance = is_instance  # of type: Anything -> bool

    @staticmethod
    def by_name(type_name):
        """Given the full name of a hlwm type, return
        the metadata
        python type"""
        for t in hlwm_types():
            if t.name == type_name:
                return t
        return None

    @staticmethod
    def by_type_of_variable(python_variable):
        """Given a variable, detect its type
        """
        for t in hlwm_types():
            if t.is_instance(python_variable):
                return t
        return None


def hlwm_types():
    """
    Return a list of HlwmType objects.

    Unfortunately, the order matters for the is_instance() predicate: Here, the
    first matching type in the list must be used. (This is because
    `isinstance(True, int)` is true)
    """
    types = [
        HlwmType('bool',
                 bool_from_user_str,
                 lambda b: 'true' if b else 'false',
                 lambda x: isinstance(x, bool)),

        HlwmType('int', int, str, lambda x: isinstance(x, int)),

        # there is no uint in python, so we just convert it to 'int'
        HlwmType('uint', int, str, lambda x: False),

        HlwmType('rectangle',
                 Rectangle.from_user_str,
                 Rectangle.to_user_str,
                 lambda x: isinstance(x, Rectangle)),

        HlwmType('string',
                 lambda x: x,
                 lambda x: x,
                 lambda x: isinstance(x, str)),
    ]

    return types


def bool_from_user_str(bool_string):
    """Parse a the string description of a hlwm boolean to
    a python boolean"""
    if bool_string.lower() in ['true', 'on']:
        return True
    if bool_string.lower() in ['false', 'off']:
        return False
    raise Exception(f'"{bool_string}" is not a boolean')
