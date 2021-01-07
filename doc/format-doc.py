#!/usr/bin/env python3
import json
import argparse
import textwrap


def count_whitespace_prefix(string):
    """return the number of spaces at the beginning of the given string"""
    for i, ch in enumerate(string):
        if ch != ' ':
            return i
    return len(string)


def multiline_for_bulletitem(src):
    """requote a multiline asciidoc doc such
    that it can be put in the item of a bullet list"""
    lines = src.splitlines()
    lastline = ''
    # add explicit markers for code blocks
    newlines = []

    def force_linebreak():
        """insert a linebreak in `newlines`"""
        if len(newlines) > 0 and newlines[-1] == '+':
            # never add two '+' next to each other
            pass
        else:
            newlines.append('+')

    codeblock_indent = 0
    for lin in lines:
        if lin.startswith('  ') and not lastline.startswith('  '):
            force_linebreak()
            newlines += ['----']  # codeblock starts with 'lin'
            codeblock_indent = count_whitespace_prefix(lin)
        if not lin.startswith('  ') and lastline.startswith('  '):
            # codeblock ended before 'lin'
            # so dedent the lines of this codeblock by `codeblock_indent`
            for i, codeblockline in reversed(list(enumerate(newlines))):
                if not codeblockline.startswith('  '):
                    break
                newlines[i] = codeblockline[codeblock_indent:]
            # end codeblock:
            newlines += ['----']
            force_linebreak()
        else:
            codeblock_indent = min(codeblock_indent, count_whitespace_prefix(lin))
        if lin == '':
            force_linebreak()
        else:
            newlines.append(lin)
        lastline = lin
    return '\n'.join(newlines)


def escape_string_value(string):
    if string == '':
        return '\"\"'
    else:
        needs_quotes = False
        for ch in '*|` ':
            if ch in string:
                needs_quotes = True
        string = string.replace('"', '\\"').replace('\'', '\\\'')
        if needs_quotes:
            return '\"{}\"'.format(string)
        else:
            return string


class ObjectDocPrinter:
    def __init__(self, jsondoc):
        self.jsondoc = jsondoc
        # a set of class names whose documentation
        # has been printed already.
        #
        # normally, the doc of class is printed when
        # the first object (in depth-first order) of that class is found in the
        # tree. But one can also insert particular paths in the following dict
        # and then the class doc will be put there.
        self.clsname2path = {}

    def class_doc_id(self, clsname):
        """for a class name, return its id in the document
        such that it can be referenced.
        """
        return 'doc_cls_' + clsname.lower()

    def reference_to_class_doc(self, clsname, path):
        """
        given a classname and a node in the tree (via path),
        return one of the following:  (i.e. either or)

        - an id and text if the doc of clsname should be referenced
        - None if the documentation should be printed here
        """
        if clsname in self.clsname2path:
            if self.clsname2path[clsname] == path:
                # print the class doc here
                return None
            else:
                # return a reference to the place of the class doc
                identifier = self.class_doc_id(clsname)
                text = '+' + '.'.join(self.clsname2path[clsname]) + '+'
                return identifier, text
        else:
            # otherwise, print the class doc here:
            self.clsname2path[clsname] = path
            return None

    def class_doc_empty(self, clsname):
        objdoc = self.jsondoc['objects'][clsname]
        return 'doc' not in objdoc \
            and len(objdoc['children']) == 0 \
            and len(objdoc['attributes']) == 0

    def run(self, clsname, path=[]):
        """print the documentation for a given class. However,
        if the documentation for it has already been generated,
        only insert a link to ot using clsname2anchor
        """
        reference_cls_doc = self.reference_to_class_doc(clsname, path)
        if reference_cls_doc is not None:
            identifier, text = reference_cls_doc
            print(f'See <<{identifier},{text}>>')
            return
        # otherwise, print it here:
        identifier = self.class_doc_id(clsname)
        depth = len(path)

        objdoc = self.jsondoc['objects'][clsname]
        print(f'[[{identifier}]]')
        if 'doc' in objdoc:
            if depth > 1:
                print(multiline_for_bulletitem(objdoc['doc']))
            else:
                print(objdoc['doc'])
        print('')
        if path == []:
            bulletprefix = ''
            ws_prefix = ''
        else:
            bulletprefix = depth * ' ' + (depth - 1) * '*'
            ws_prefix = depth * ' ' + '   '  # whitespace prefix
        for _, attr in objdoc['attributes'].items():
            if attr['default_value'] is not None:
                default_val = '= ' + escape_string_value(attr['default_value'])
            else:
                default_val = ''
            if attr.get('doc', None) is not None:
                docstr = attr.get('doc', None)
            else:
                docstr = ''
            print(f"{ws_prefix}{bulletprefix}* {attr['type']} +{attr['name']}+ {default_val} {docstr}")
        for _, child in objdoc['children'].items():
            docstr = ': ' + child['doc'].strip() if 'doc' in child else ''
            # class_doc = self.jsondoc['objects'][child['type']].get('doc', '')
            if len(docstr) > 0 and not docstr.endswith('.'):
                docstr += '.'
            if depth > 0:
                itemname = f"+{child['name']}+"
                bullet = '*'
            else:
                itemname = f"{child['name']}"
                bullet = '\n==='
            if depth == 0 and self.class_doc_empty(child['type']):
                # do not list subsystems that are entirely empty
                # at the moment
                continue
            print(f"{ws_prefix}{bulletprefix}{bullet} {itemname} {docstr} ", end='')
            self.run(child['type'], path=path + [child['name']])


def main():
    parser = argparse.ArgumentParser(description='Generate object documentation')
    parser.add_argument('jsondoc', help='the hlwm-doc.json file')

    args = parser.parse_args()

    with open(args.jsondoc, 'r') as fh:
        jsondoc = json.load(fh)

    print(textwrap.dedent("""
    herbstluftwm-objects(7)
    =======================

    NAME
    ----
    herbstluftwm-objects - the object system

    DESCRIPTION
    -----------
    The state of herbstluftwm can interactively be introspected
    and modified via the object system. Similarly to a file system,
    the objects are organized in a tree:

    """))

    doc_printer = ObjectDocPrinter(jsondoc)
    doc_printer.clsname2path.update({
        'Client': ['clients', 'focus'],
        'FrameLeaf': ['tags', 'focus', 'tiling', 'focused_frame'],
    })
    doc_printer.run('Root')


main()
