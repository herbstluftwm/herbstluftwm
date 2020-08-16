#!/usr/bin/env python3

import argparse
import os
import sys
import re


def findfiles(sourcedir, regex_object):
    """find all files in the given 'sourcedir' whose
    filename matches 'regex_object'
    """
    for root, dirs, files in os.walk(sourcedir):
        for file in files:
            if regex_object.match(file):
                 yield os.path.join(root, file)


def extract_file_tokens(filepath):
    # in the following, it's important to use
    # non-capturing groups (?: .... )
    token_types = [
        # the following is complicated because we allow to
        # escape newlines for longer macro definitions, but
        # the \ may also occur within the macro definition
        # hence the following regex checks that there is no
        # \ right before the last \n
        "#(?:[^Z\n]|\\\\\n)*[^\\\\]\n",  # preprocessor
        '//[^\n]*\n',  # single-line comment
        '/\*(?:[^\*]*|\**[^/*])*\*/',  # multiline comment
        '[a-zA-Z_][a-zA-Z0-9_]*',  # identifiers
        '[0-9][0-9\.a-z]*', # numbers
        '\'(?:\\\'|[^\']*)\'',
        "\"(?:\\\"|[^\"]*)\"",
        "[-+<>/*]",  # operators
        r'[\(\),;:{}\[\]\?&|~]',
        '[\t ][\t ]*',
        '\n',
    ]
    entire_regex = ['(?:{})'.format(t) for t in token_types]
    entire_regex = re.compile('(' + '|'.join(entire_regex) + ')')
    with open(filepath, 'r') as fh:
        for t in entire_regex.split(fh.read().replace('\r', '') + '\n'):
            if t is not None and t.strip(' \t') != '':
                yield t


def pretty_print_token_list(tokens, indent='  '):
    """print all the tokens and auto-indent based on { }"""
    number_nested_braces = 0
    first_in_line = True
    line_number = 1
    for tok in tokens:
        if tok == '}':
            number_nested_braces -= 1
        linecontent = tok.rstrip('\n')
        if linecontent != '':
            if first_in_line:
                print('{:4}: '.format(line_number) + number_nested_braces * indent, end='')
            print(' \'{}\''.format(linecontent), end='')
        for ch in tok:
            if ch == '\n':
                line_number += 1
        if tok.endswith('\n'):
            print('')
            first_in_line = True
        else:
            first_in_line = False
        if tok == '{':
            number_nested_braces += 1
    print()


class TokenTree:
    """A list of TokenTrees is a list whose elements are either:
        - plain strings (something like a 'class' token)
        - a token group enclosed by ( ), [ ], or { } tokens
    """
    def __init__(self):
        """this should not be called directly"""
        # token group
        self.opening_token = None
        self.enclosed_tokens = []  # list of TokenTree objects
        self.closing_token = None
        pass

    @staticmethod
    def LiterateToken(tok):
        """just return a plain string"""
        return tok

    @staticmethod
    def TokenGroup(opening, enclosed, closing):
        tt = TokenTree()
        tt.opening_token = opening
        tt.enclosed_tokens = enclosed
        tt.closing_token = closing
        return tt

    @staticmethod
    def IsTokenGroup(tokenStringOrGroup):
        if isinstance(tokenStringOrGroup, TokenTree):
            return True
        else:
            return False

    def __str__(self):
        return '{} ... {}'.format(self.opening_token, self.closing_token)

    @staticmethod
    def PrettyPrintList(tokentree_list, curindent=''):
        for t in tokentree_list:
            if TokenTree.IsTokenGroup(t):
                print(curindent + t.opening_token)
                TokenTree.PrettyPrintList(t.enclosed_tokens, curindent=curindent + '  ')
                print(curindent + t.closing_token)
            else:
                if t.strip() == '':
                    continue
                print(curindent + t)


class TokenStream:
    """a stream of objects"""
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0
        self.re_type = type(re.compile('the type of regexes'))

    def empty(self):
        return self.pos >= len(self.tokens)

    def pop(self, error_message="expected another token"):
        """pop a token from the stream. If the stream is empty
        already, raise an exception instead
        """
        if self.pos >= len(self.tokens):
            fullmsg = "Unexpected end of token stream: {}"
            fullmsg = fullmsg.format(error_message)
            raise Exception(fullmsg)
        tok = self.tokens[self.pos]
        # print("tok {}".format(tok), file=sys.stderr)
        self.pos += 1
        # print("yielding token '{}'".format(t))
        return tok

    def undo_pop(self):
        """undo the last pop operation"""
        # print("tokundo", file=sys.stderr)
        self.pos -= 1

    class PatternArg:
        """a PatternArg object passed to try_match()
        allows the try_match function to return one or multiple tokens
        """
        def __init__(self):
            self.value = None

        def assign(self, value):
            self.value = value

    def try_match(self, *args):
        """if the next tokens match the list in the *args
        then pop them from the stream, else do nothing.
        if one of the next tokens is not a string, the matching
        returns False.
        """
        for delta, pattern in enumerate(args):
            if self.pos + delta >= len(self.tokens):
                return False

            curtok = self.tokens[self.pos + delta]
            if not isinstance(curtok, str):
                # never match a token group
                return False
            elif isinstance(pattern, self.re_type):
                if not pattern.match(curtok):
                    return False
            elif isinstance(pattern, str):
                if pattern != curtok:
                    return False
            elif isinstance(pattern, TokenStream.PatternArg):
                pattern.assign(curtok)
            else:
                raise Exception("unknown pattern type {}".format(type(pattern)))
        self.pos += len(args)
        return True

    def assert_match(self, *args, msg=''):
        m = self.try_match(*args)
        if not m:
            text = "failed to match »"
            text += ' '.join(args)
            text += "« with the current stream:\n"
            # show some context before
            beginpos = max(0, self.pos - 5)
            # show some context afterwards
            endpos = min(len(self.tokens), self.pos + max(4, len(args) + 1))
            # show some cursor in the next line
            nextline = ''
            for t in range(beginpos, endpos):
                if t != beginpos:
                    text += ' '
                    nextline += ' '
                text += str(self.tokens[t])
                if t == self.pos:
                    nextline += '^ here'
                else:
                    nextline += ' ' * len(self.tokens[t])
            text += '\n' + nextline
            if msg != '':
                text += "\n'{}'".format(msg)
            raise Exception(text)


    def discard_until(self, *args):
        """
        discard tokens until try_match(*args) succeeds (returning True)
        or until the stream is empty (returning False)
        """
        while not self.empty():
            # terminate if the args match to the upcoming tokens
            if self.try_match(';'):
                return True
            # otherwise, discard the next token:
            self.pop()
        return False


def build_token_tree_list(token_stream):
    """return a list of TokenTree objects"""
    matching = [
        ('(', ')'),
        ('{', '}'),
        ('[', ']'),
    ]
    while not token_stream.empty():
        t = token_stream.pop()
        if t in [m[1] for m in matching]:  # closing scope
            token_stream.undo_pop()
            break
        elif t in [m[0] for m in matching]:  # opening scope
            nested = list(build_token_tree_list(token_stream))
            closing = token_stream.pop()
            if (t, closing) not in matching:
                raise Exception("'{}' is closed by '{}'".format(t, closing))
            yield TokenTree.TokenGroup(t, nested, closing)
        else:
            yield TokenTree.LiterateToken(t)


class ObjectInformation:
    def __init__(self):
        self.base_classes = {}  # mapping class names to base clases

    def base_class(self, subclass, baseclass):
        if subclass not in self.base_classes:
            self.base_classes[subclass] = []
        self.base_classes[subclass].append(baseclass)

    def print(self):
        for k, v in self.base_classes.items():
            bases = [str(b) for b in v]
            print("{} has the base classes: \'{}\'".format(k, '\' \''.join(bases)))


class ClassName:
    def __init__(self, name, type_modifier=[], namespace=[], template_args=[]):
        self.type_modifier = type_modifier  # something like 'unsigned'
        self.namespace = namespace
        self.name = name
        self.template_args = template_args

    def __str__(self):
        s = ''
        for t in self.type_modifier:
            s += t + ' '
        for n in self.namespace:
            s += n + '::'
        s += self.name
        if len(self.template_args) > 0:
            s += '<' + ','.join(self.template_args) + '>'
        return s


class TokTreeInfoExtrator:
    """given a token tree list as returned by
    build_token_tree_list(), extract all the object
    information and pass it over to the given
    ObjectInformation object.

    The actual extraction is done in the main()
    function which has to be called separatedly
    """

    def __init__(self, objInfo):
        self.objInfo = objInfo

    def stream_pop_class_name(self, stream):
        type_modifier = []
        namespace = []
        if stream.try_match('unsigned'):
            type_modifier.append('unsigned')
        if stream.try_match(':', ':'):
            # absolute namespace, e.g. ::std::exception.
            namespace.append('')
        name = stream.pop('expected class name')
        tmpl_args = []
        arg = TokenStream.PatternArg()
        while stream.try_match(':', ':', arg):
            namespace.append(name)
            name = arg.value
        if stream.try_match('<'):
            tok = stream.pop('expected template argument')
            tmpl_args.append(tok)
            while stream.try_match(','):
                tmpl_args.append(stream.pop('expected template argument'))
            stream.assert_match('>',
                                msg='expecting > after last template argument')
        return ClassName(name, type_modifier=type_modifier,
                         namespace=namespace, template_args=tmpl_args)

    def inside_class_definition(self, toktreelist, classname):
        """
        go on parsing inside the body of a class
        """
        pub_priv_prot_re = re.compile('public|private|protected')
        stream = TokenStream(toktreelist)
        while not stream.empty():
            if stream.try_match(pub_priv_prot_re, ':'):
                continue
            # whenever we reach this point, this is a new
            # member variable or member function definition
            elif stream.try_match('using'):
                semicolon_found = stream.discard_until(';')
                assert semicolon_found, "expected ; after 'using'"
            elif stream.try_match('Attribute_', '<'):
                attr_type = self.stream_pop_class_name(stream)
                assert stream.try_match('>'), \
                    "every 'Attribute_<' has to be closed by '>'"
                attr_name = stream.pop("expect an attribute name");
                print("attribute " + attr_name)
                after_attr_name = stream.pop()
                if stream.try_match('='):
                    # static initializiation:
                    t = stream.pop()
                    assert TokenTree.IsTokenGroup(t)
                if stream.try_match(';'):
                    # end of attribute definition
                    pass
                else:
                    # some other definition (e.g. a function)
                    stream.discard_until(';')
            else:
                stream.discard_until(';')

    def main(self, toktreelist):
        """extract object information from a list of TokenTree objects
        and save the data in the ObjectInformation object passed"""
        # pub_priv_prot = ['public', 'private', 'protected']
        pub_priv_prot_re = re.compile('public|private|protected')
        arg1 = TokenStream.PatternArg()
        stream = TokenStream(toktreelist)
        while not stream.empty():
            if stream.try_match('class', arg1):
                classname = arg1.value
                while stream.try_match(re.compile('^,|:$'), pub_priv_prot_re):
                    baseclass = self.stream_pop_class_name(stream)
                    self.objInfo.base_class(classname, baseclass)
                # scan everything til the final ';'
                while not stream.empty():
                    t = stream.pop()
                    if TokenTree.IsTokenGroup(t):
                        self.inside_class_definition(t.enclosed_tokens, classname)
                    elif t == ';':
                        break
            else:
                stream.pop()


def main():
    parser = argparse.ArgumentParser(description='extract hlwm doc from the source code')
    parser.add_argument('--sourcedir', default='./src/',
                        help='directory containing the source files')
    parser.add_argument('--fileregex', default='.*\.(h|cpp)$',
                        help='consider files whose name matches this regex')
    parser.add_argument('--tokenize-single-file',
                        help='tokenize a particular file and then exit')
    parser.add_argument('--tokentree-single-file',
                        help='print the token tree of a particular file and then exit')
    parser.add_argument('--objects', action='store_const', default=False, const=True,
                        help='extract object information')
    args = parser.parse_args()

    # only evaluated if needed:
    files = lambda: findfiles(args.sourcedir, re.compile(args.fileregex))

    if args.tokenize_single_file is not None:
        tokens = extract_file_tokens(args.tokenize_single_file)
        pretty_print_token_list(tokens)
        return 0

    if args.tokentree_single_file is not None:
        tokens = extract_file_tokens(args.tokentree_single_file)
        stream = TokenStream(list(tokens))
        tt_list = list(build_token_tree_list(stream))
        if not stream.empty():
            print('Warning: unmatched [ ], ( ), { }', file=sys.stderr)
            print("Remaining tokens are:", file=sys.stderr)
            while not stream.empty():
                print("token: {}".format(stream.pop()), file=sys.stderr)
        TokenTree.PrettyPrintList(tt_list)
        return 0

    if args.objects is not None:
        objInfo = ObjectInformation()
        for f in files():
            print("parsing file {}".format(f))
            toks = [t for t in extract_file_tokens(f) if t.strip() != '']
            toktree = list(build_token_tree_list(TokenStream(toks)))
            extractor = TokTreeInfoExtrator(objInfo)
            extractor.main(list(toktree))
        objInfo.print()


EXITCODE = main()
if EXITCODE is not None:
    sys.exit(EXITCODE)
