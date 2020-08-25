#!/usr/bin/env python3

import argparse
import os
import sys
import re
import ast
import json


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


class TokenGroup:
    """
    A TokenGroup represents a list of tokens that are enclosed by ( ), [ ], or { }.
    A 'list of tokens' is actually a list whose elements are strings or
    TokenGroup objects.
    """
    def __init__(self):
        """this should not be called directly"""
        # token group
        self.opening_token = None
        self.enclosed_tokens = []  # list of strings or TokenGroup objects
        self.closing_token = None
        pass

    @staticmethod
    def LiterateToken(tok):
        """just return a plain string"""
        return tok

    @staticmethod
    def TokenGroup(opening, enclosed, closing):
        tt = TokenGroup()
        tt.opening_token = opening
        tt.enclosed_tokens = enclosed
        tt.closing_token = closing
        return tt

    @staticmethod
    def IsTokenGroup(tokenStringOrGroup, opening_token=None):
        """returns whether the given object is a token group.
        if an 'opening_token' is given, it is additionally checked
        whether the opening_token matches"""
        if isinstance(tokenStringOrGroup, TokenGroup):
            if opening_token is not None:
                return tokenStringOrGroup.opening_token == opening_token
            else:
                return True
        else:
            return False

    def __str__(self):
        return '{}...{}'.format(self.opening_token, self.closing_token)

    @staticmethod
    def PrettyPrintList(tokentree_list, curindent=''):
        for t in tokentree_list:
            if TokenGroup.IsTokenGroup(t):
                print(curindent + t.opening_token)
                TokenGroup.PrettyPrintList(t.enclosed_tokens, curindent=curindent + '  ')
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
        allows the try_match function to return one or multiple tokens.

        If a regex is given, then the assign only succeeds if the value
        is a string and matches the regex.

        If a callback is given, then the assing only succeeds if the callback
        maps the value to 'True'. The default callback verifies that the
        token is a string
        """
        def __init__(self, re=None, callback=lambda x: isinstance(x, str)):
            self.value = None
            self.regex = re
            self.callback = callback

        def assign(self, value):
            if self.regex is not None:
                if not isinstance(value, str):
                    return False
                if not self.regex.match(value):
                    return False
            if self.callback is not None:
                if self.callback(value) is not True:
                    return False
            self.value = value
            return True

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
            if isinstance(pattern, self.re_type):
                if not isinstance(curtok, str) or not pattern.match(curtok):
                    return False
            elif isinstance(pattern, str):
                if pattern != curtok:
                    return False
            elif isinstance(pattern, TokenStream.PatternArg):
                if not pattern.assign(curtok):
                    return False
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
            if self.try_match(*args):
                return True
            # otherwise, discard the next token:
            self.pop()
        return False


def build_token_tree_list(token_stream):
    """return a list of strings or TokenGroup objects"""
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
            yield TokenGroup.TokenGroup(t, nested, closing)
        else:
            yield TokenGroup.LiterateToken(t)


class ClassName:
    """a class name with additional information of the surrounding namespace
    and template arguments"""
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

    def to_user_type_name(self):
        if self.name == 'string':
            return 'string'
        if self.name == 'Color':
            return 'color'
        if self.type_modifier == ['unsigned'] and self.name == 'long':
            return 'uint'
        if self.name == 'RegexStr':
            return 'regex'
        if self.name == 'FixPrecDec':
            return 'decimal'
        return self.__str__()


class ObjectInformation:
    """This gathers all kinds of information about hlwm's object tree"""
    class AttributeInformation:
        def __init__(self, cpp_name):
            self.cpp_name = cpp_name
            self.type = None
            self.user_name = None  # the name that is visible to the user
            self.default_value = None
            self.attribute_class = None  # whether this is an Attribute_ or sth else
            self.constructor_args = None  # the arguments to the constructor

        def add_constructor_args(self, args):
            # split 'args' by the ',' tokens
            new_args = []
            current_chunk = []
            for a in args + [',']:  # simulate the end by an artifical ','
                if str(a) == ',':
                    if len(current_chunk) == 1:
                        # if there was one token before the ',' (or end)
                        # then only put this token into the 'new_args'
                        new_args.append(current_chunk[0])
                    else:
                        # if there were multiple or no tokens, then
                        # retain the list
                        new_args.append(current_chunk)
                    current_chunk = []
                else:
                    current_chunk.append(a)

            args = new_args  # also update 'args' for shorter notation later
            self.constructor_args = args
            if self.attribute_class is None:
                return
            if self.attribute_class == 'Attribute_':
                if len(args) == 2:
                    self.set_user_name(args[0])
                    self.set_default_value(args[1])
                elif len(args) >= 3:
                    self.set_user_name(args[1])
                    self.set_default_value(args[2])
            if self.attribute_class == 'DynAttribute_':
                if len(args) == 2:
                    self.set_user_name(args[0])
                elif len(args) >= 3 and args[0] == 'this':
                    self.set_user_name(args[1])
                elif len(args) >= 3 and args[0] != 'this':
                    self.set_user_name(args[0])
            if self.attribute_class == 'AttributeProxy_':
                self.set_user_name(args[0])
                self.set_default_value(args[1])

        def set_user_name(self, cpp_token):
            if cpp_token[0:1] == '"':
                # evaluate quotes
                cpp_token = ast.literal_eval(cpp_token)
            self.user_name = cpp_token

        def set_default_value(self, cpp_token):
            if TokenGroup.IsTokenGroup(cpp_token):
                # drop surrounding '{' ... '}' if its an initalizer list
                cpp_token = cpp_token.enclosed_tokens[0]
            if cpp_token[0:1] == ['-']:
                # this is most probably a signed number
                cpp_token = ''.join(cpp_token)
            if cpp_token[0:4] == ['RegexStr', ':', ':', 'fromStr']:
                # assume that the next token in the list 'cpp_token' is a
                # token group
                cpp_token = cpp_token[4].enclosed_tokens[0]
            if cpp_token == 'WINDOW_MANAGER_NAME':
                cpp_token = 'herbstluftwm'
            if cpp_token[0:1] == '"':
                # evaluate quotes
                cpp_token = ast.literal_eval(cpp_token)
            self.default_value = cpp_token

    class ChildInformation:
        def __init__(self, cpp_name):
            self.cpp_name = cpp_name
            self.user_name = None  # what the user sees
            self.child_class = None  # whether this is a Link_ or a Child_
            self.type = None  # the template argument to Link_ or Child_
            self.constructor_args = None

        def add_constructor_args(self, args):
            if len(args) >= 4:
                cpp_token = args[3]
                if cpp_token == 'TMP_OBJECT_PATH':
                    cpp_token = 'tmp'
                if cpp_token[0:1] == '"':
                    cpp_token = cpp_token[1:-1]
                self.user_name = cpp_token

    def __init__(self):
        self.base_classes = {}  # mapping class names to base clases
        self.member2info = {}  # mapping (classname,member) to ChildInformation or AttributeInformation
        self.member2init = {}  # mapping (classname,member) to its initalizer list

    def base_class(self, subclass, baseclass):
        if subclass not in self.base_classes:
            self.base_classes[subclass] = []
        self.base_classes[subclass].append(baseclass)

    def member_init(self, classname: str, member: str, init_list):
        """the classname::member is initialized by the given 'init_list'"""
        self.member2init[(classname, member)] = init_list

    def process_member_init(self):
        """try to pass colleced member initializations to attributes"""
        for (clsname, attrs), attr in self.member2info.items():
            if attr.constructor_args is not None:
                continue
            init_list = self.member2init.get((clsname, attr.cpp_name), None)
            if init_list is not None:
                attr.add_constructor_args(init_list)

    def attribute_info(self, classname: str, attr_cpp_name: str):
        """return the AttributeInformation object for
        a for a class and its attribute whose C++ variable name is
        'attr_cpp_name'. Create the object if necessary
        """
        if (classname, attr_cpp_name) not in self.member2info:
            self.member2info[(classname, attr_cpp_name)] = \
                ObjectInformation.AttributeInformation(attr_cpp_name)
        return self.member2info[(classname, attr_cpp_name)]

    def child_info(self, classname: str, cpp_name: str):
        """return the ChildInformation object for
        a for a class and its child whose C++ variable name is
        'cpp_name'. Create the object if necessary
        """
        if (classname, cpp_name) not in self.member2info:
            self.member2info[(classname, cpp_name)] = \
                ObjectInformation.ChildInformation(cpp_name)
        return self.member2info[(classname, cpp_name)]

    def superclasses_transitive(self):
        """return a set of a dict mapping a class to the set
        of it's transitive superclasses"""
        def bounded_depth_first_search(clsname, target_dict):
            """collect all super classes for 'clsname' and put it into
            the target_dict"""
            # check this first to avoid cycles
            if clsname in target_dict:
                # nothing to do
                return
            bases = [b.name for b in self.base_classes.get(clsname, [])]
            target_dict[clsname] = bases
            for b in bases:
                bounded_depth_first_search(b, target_dict)
                target_dict[clsname] += target_dict[b]

        cls2supers = {}
        for cls in self.base_classes:
            bounded_depth_first_search(cls, cls2supers)
        return cls2supers

    def print(self):
        for k, v in self.base_classes.items():
            bases = [str(b) for b in v]
            print("{} has the base classes: \'{}\'".format(k, '\' \''.join(bases)))
            attributes = []
            for (cls, member), attr in self.member2info.items():
                if cls == k:
                    attributes.append(attr)
            attributes = sorted(attributes, key=lambda a: a.user_name)
            if len(attributes) > 0:
                print("{} has the attributes:".format(k))
                for attr in attributes:
                    line = '  ' + str(attr.user_name)
                    key2value = [
                        ('cpp_name', attr.cpp_name),
                        ('type', attr.type),
                        ('default_value', attr.default_value),
                        ('class', attr.attribute_class),
                    ]
                    for key_str, value in key2value:
                        if value is not None:
                            # properly quote string values
                            if isinstance(value, str):
                                # convert a list to a string, such that 'value' gets quoted
                                # in the usual python syntax. and then we remove the first
                                # and last characters which are the [ ] coming from the list:
                                value = str([value])[1:-1]
                            line += '  {}={}'.format(key_str, value)
                    print(line)

    def json_object(self):
        # 1. collect all subclasses of 'Object'
        superclasses = self.superclasses_transitive()
        result = {}

        # 2. create a helper dict mapping classes to its members
        cls2members = {}
        for (cls, _), member in self.member2info.items():
            if cls not in cls2members:
                cls2members[cls] = []
            cls2members[cls].append(member)

        # 3. create the json-object with all object/attribute info
        for clsname, supers in superclasses.items():
            if 'Object' not in supers:
                continue
            attributes = {}
            children = {}
            for cls in [clsname] + supers:
                for member in cls2members.get(cls, []):
                    if isinstance(member, ObjectInformation.AttributeInformation):
                        # assert uniqueness:
                        assert member.user_name not in attributes
                        attributes[member.user_name] = {
                            'name': member.user_name,
                            'cpp_name': member.cpp_name,
                            'type': member.type.to_user_type_name(),
                            'default_value': member.default_value,
                            'class': member.attribute_class,
                        }
                    if isinstance(member, ObjectInformation.ChildInformation):
                        # assert uniqueness:
                        assert member.user_name not in children
                        children[member.user_name] = {
                            'name': member.user_name,
                            'cpp_name': member.cpp_name,
                            'type': member.type.to_user_type_name(),
                            'class': member.child_class,
                        }
            assert clsname not in result  # assert uniqueness
            result[clsname] = {
                'classname': clsname,
                'children': children,
                'attributes': attributes,
            }
        return {'objects': result}  # only generate object doc so far


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
        attr_cls_re = re.compile('^(Dyn|)Attribute(Proxy|)_$')
        attribute_ = TokenStream.PatternArg(re=attr_cls_re)
        link_ = TokenStream.PatternArg(re=re.compile('^(Link_|Child_)$'))
        parameters = TokenStream.PatternArg(callback=lambda t: TokenGroup.IsTokenGroup(t, opening_token='('))
        def semicolon_or_block_callback(t):
            return t == ';' or TokenGroup.IsTokenGroup(t, opening_token='{')
        semicolon_or_block = TokenStream.PatternArg(callback=semicolon_or_block_callback)
        arg = TokenStream.PatternArg()
        while not stream.empty():
            if stream.try_match(pub_priv_prot_re, ':'):
                continue
            if stream.try_match(re.compile('^(//|/\*)')):
                # skip comments
                continue
            # whenever we reach this point, this is a new
            # member variable or member function definition
            elif stream.try_match('using'):
                semicolon_found = stream.discard_until(';')
                assert semicolon_found, "expected ; after 'using'"
            elif stream.try_match(attribute_, '<'):
                attr_type = self.stream_pop_class_name(stream)
                assert stream.try_match('>'), \
                    "every 'Attribute_<' has to be closed by '>'"
                attr_name = stream.pop("expect an attribute name")
                attr = self.objInfo.attribute_info(classname, attr_name)
                attr.type = attr_type
                attr.attribute_class = attribute_.value
                if stream.try_match('='):
                    # static initializiation:
                    t = stream.pop()
                    assert TokenGroup.IsTokenGroup(t)
                    attr.add_constructor_args(t.enclosed_tokens)
                if stream.try_match(';'):
                    # end of attribute definition
                    pass
                else:
                    # some other definition (e.g. a function)
                    stream.discard_until(semicolon_or_block)
            elif stream.try_match(link_, '<'):
                link_type = self.stream_pop_class_name(stream)
                stream.assert_match('>', msg="every 'Link_<' has to be enclosed by '>'")
                cpp_name = stream.pop("expect an attribute name")
                link = self.objInfo.child_info(classname, cpp_name)
                link.child_class = link_.value
                link.type = link_type
                stream.discard_until(semicolon_or_block)
            elif stream.try_match('ByName', arg):
                link = self.objInfo.child_info(classname, arg.value)
                link.child_class = 'ByName'
                link.user_name = 'by-name'
                link.type = ClassName('ByName')
                stream.discard_until(semicolon_or_block)
            elif stream.try_match(classname, parameters, ':'):
                self.stream_consume_member_initializers(classname, stream)
            else:
                stream.discard_until(semicolon_or_block)

    def stream_consume_member_initializers(self, classname, stream):
        """given that the opening : is already consumed, consume the member
        initializations"""
        arg1 = TokenStream.PatternArg()
        codeblock = TokenStream.PatternArg(callback=lambda t: TokenGroup.IsTokenGroup(t, opening_token='{'))
        parameters = TokenStream.PatternArg(callback=lambda t: TokenGroup.IsTokenGroup(t, opening_token='('))
        while not stream.try_match(codeblock):
            if stream.try_match(arg1, parameters):
                # we found a member initialization
                init_list = parameters.value.enclosed_tokens
                self.objInfo.member_init(classname, arg1.value, init_list)
            else:
                stream.pop()

    def main(self, toktreelist):
        """extract object information from a list of strings/TokenGroup objects
        and save the data in the ObjectInformation object passed"""
        # pub_priv_prot = ['public', 'private', 'protected']
        pub_priv_prot_re = re.compile('public|private|protected')
        arg1 = TokenStream.PatternArg()
        arg2 = TokenStream.PatternArg()
        stream = TokenStream(toktreelist)
        parameters = TokenStream.PatternArg(callback=lambda t: TokenGroup.IsTokenGroup(t, opening_token='('))
        while not stream.empty():
            if stream.try_match('class', arg1):
                classname = arg1.value
                while stream.try_match(re.compile('^,|:$'), pub_priv_prot_re):
                    baseclass = self.stream_pop_class_name(stream)
                    self.objInfo.base_class(classname, baseclass)
                # scan everything til the final ';'
                while not stream.empty():
                    t = stream.pop()
                    if TokenGroup.IsTokenGroup(t):
                        self.inside_class_definition(t.enclosed_tokens, classname)
                    elif t == ';':
                        break
            elif stream.try_match(arg1, ':', ':', arg2, parameters, ':'):
                if arg1.value != arg2.value:
                    continue
                classname = arg1.value
                # we found the constructor for 'classname'
                self.stream_consume_member_initializers(classname, stream)
            else:
                stream.pop()
        # pass the member initializations to the attributes:
        self.objInfo.process_member_init()


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
    parser.add_argument('--json', action='store_const', default=False, const=True,
                        help='print object info as json')
    args = parser.parse_args()

    # only evaluated if needed:
    def files():
        return findfiles(args.sourcedir, re.compile(args.fileregex))

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
        TokenGroup.PrettyPrintList(tt_list)
        return 0

    if args.objects or args.json:
        objInfo = ObjectInformation()
        for f in files():
            # print("parsing file {}".format(f), file=sys.stderr)
            toks = [t for t in extract_file_tokens(f) if t.strip() != '']
            toktree = list(build_token_tree_list(TokenStream(toks)))
            extractor = TokTreeInfoExtrator(objInfo)
            extractor.main(list(toktree))
        if args.objects:
            objInfo.print()
        else:
            print(json.dumps(objInfo.json_object(), indent=2))


EXITCODE = main()
if EXITCODE is not None:
    sys.exit(EXITCODE)
