#!/usr/bin/env python3
import json
import re
import argparse


def count_whitespace_prefix(string):
    """return the number of spaces at the beginning of the given string"""
    for i, ch in enumerate(string):
        if ch != ' ':
            return i
    return len(string)


def cpp_source_doc_to_asciidoc(src):
    """the doc in the cpp source sometimes can not be fully
    asciidoc compatible because it is also used for plain-text
    documentation in the 'help' command.

    Thus, this function adds some more escape sequences to avoid
    unintended asciidoc markup
    """
    # prevent any formatting within a single-quoted string.
    src = re.sub(r"'([^']*[^\\])'", r"'+++\1+++'", src)

    # indent any nested bullet items correctly
    src = re.sub('\n  \*', '\n  **', src)
    return src


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


def splitcamelcase(string):
    """Transform CamelCase into camel case"""
    res = ""
    for ch in string:
        if re.match('[A-Z]', ch):
            res += ' ' + ch.lower()
        else:
            res += ch
    return res


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

        # the classes that are abstract. For abstract classes,
        # we don't print the doc but instead the doc of all its
        # implementing classes
        self.abstractclass = set()

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
            print(f'For attributes and children, see <<{identifier},{text}>>')
            return
        # otherwise, print it here:
        identifier = self.class_doc_id(clsname)
        depth = len(path)

        objdoc = self.jsondoc['objects'][clsname]
        print(f'[[{identifier}]]', end='' if depth > 1 else '\n')
        if 'doc' in objdoc:
            doc_txt = cpp_source_doc_to_asciidoc(objdoc['doc'])
            if depth > 1:
                print(multiline_for_bulletitem(doc_txt))
            else:
                print(doc_txt)
        print('')
        if path == []:
            bulletprefix = ''
            ws_prefix = ''
        else:
            bulletprefix = depth * ' ' + (depth - 1) * '*'
            ws_prefix = depth * ' ' + '   '  # whitespace prefix
        for _, attr in objdoc['attributes'].items():
            if attr['default_value'] is not None:
                default_val = ' [defaultvalue]#= ' + escape_string_value(attr['default_value']) + '#'
            else:
                default_val = ''
            if attr.get('doc', None) is not None:
                docstr = ': ' + cpp_source_doc_to_asciidoc(attr['doc'])
            else:
                docstr = ''
            # add multiple formats to the entry name such that the colors work
            # both in html and in the man page output
            print(f"{ws_prefix}{bulletprefix}* '[datatype]#{attr['type']}#' *+[entryname]#{attr['name']}#+*{default_val}{docstr}")
        for _, child in objdoc['children'].items():
            docstr = cpp_source_doc_to_asciidoc(child['doc'].strip()) \
                     if 'doc' in child else ''
            # class_doc = self.jsondoc['objects'][child['type']].get('doc', '')
            if len(docstr) > 0:
                if not docstr.endswith('.'):
                    docstr += '.'
                docstr += ' '
            if depth > 0:
                # add multiple format indicators, as for the
                # attribute name above
                if child['name'] is not None:
                    itemname = f"*+[entryname]#{child['name']}#+*"
                else:
                    itemname = f"'[entryname]#{child['name_pattern']}#'"
                bullet = '*'
            else:
                itemname = f"{child['name']}"
                bullet = '\n==='
            if depth == 0 and self.class_doc_empty(child['type']):
                # do not list subsystems that are entirely empty
                # at the moment
                continue
            if child['type'] not in self.abstractclass:
                print(f"{ws_prefix}{bulletprefix}{bullet} {itemname}: {docstr}", end='')
                self.run(child['type'], path=path + [child['name']])
            else:
                for _, subclass in self.jsondoc['objects'].items():
                    if child['type'] in subclass['inherits-from']:
                        classname = splitcamelcase(subclass['classname'])
                        print(f"{ws_prefix}{bulletprefix}{bullet} {itemname} can be a {classname}. {docstr} ", end='')
                        self.run(subclass['classname'], path=path + [child['name']])


def main():
    parser = argparse.ArgumentParser(description='Generate object documentation')
    parser.add_argument('jsondoc', help='the hlwm-doc.json file')

    args = parser.parse_args()

    with open(args.jsondoc, 'r') as fh:
        jsondoc = json.load(fh)

    doc_printer = ObjectDocPrinter(jsondoc)
    doc_printer.abstractclass.add('Frame')
    doc_printer.clsname2path.update({
        'Client': ['clients', 'focus'],
        'FrameLeaf': ['tags', 'focus', 'tiling', 'root'],
    })
    doc_printer.run('Root')


main()
