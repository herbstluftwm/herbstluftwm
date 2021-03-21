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
