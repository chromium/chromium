#!/usr/bin/env vpython3
# -*- coding: ascii -*-
r"""
=====================
 Javascript Minifier
=====================

rJSmin is a javascript minifier written in python.

The minifier is based on the semantics of `jsmin.c by Douglas Crockford`_\\.

:Copyright:

 Copyright 2011 - 2015
 Andr\xe9 Malo or his licensors, as applicable

:License:

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

The module is a re-implementation aiming for speed, so it can be used at
runtime (rather than during a preprocessing step). Usually it produces the
same results as the original ``jsmin.c``. It differs in the following ways:

- there is no error detection: unterminated string, regex and comment
  literals are treated as regular javascript code and minified as such.
- Control characters inside string and regex literals are left untouched; they
  are not converted to spaces (nor to \\n)
- Newline characters are not allowed inside string and regex literals, except
  for line continuations in string literals (ECMA-5).
- "return /regex/" is recognized correctly.
- Line terminators after regex literals are handled more sensibly
- "+ +" and "- -" sequences are not collapsed to '++' or '--'
- Newlines before ! operators are removed more sensibly
- Comments starting with an exclamation mark (``!``) can be kept optionally
- rJSmin does not handle streams, but only complete strings. (However, the
  module provides a "streamy" interface).

Since most parts of the logic are handled by the regex engine it's way faster
than the original python port of ``jsmin.c`` by Baruch Even. The speed factor
varies between about 6 and 55 depending on input and python version (it gets
faster the more compressed the input already is). Compared to the
speed-refactored python port by Dave St.Germain the performance gain is less
dramatic but still between 3 and 50 (for huge inputs). See the docs/BENCHMARKS
file for details.

rjsmin.c is a reimplementation of rjsmin.py in C and speeds it up even more.

Both python 2 and python 3 are supported.

.. _jsmin.c by Douglas Crockford:
   http://www.crockford.com/javascript/jsmin.c
"""
if __doc__:
    # pylint: disable = redefined-builtin
    __doc__ = __doc__.encode('ascii').decode('unicode_escape')
__author__ = r"Andr\xe9 Malo".encode('ascii').decode('unicode_escape')
__docformat__ = "restructuredtext en"
__license__ = "Apache License, Version 2.0"
__version__ = '1.0.12'
__all__ = ['jsmin']

import re as _re


def _make_jsmin(python_only=False):
    """
    Generate JS minifier based on `jsmin.c by Douglas Crockford`_

    .. _jsmin.c by Douglas Crockford:
       http://www.crockford.com/javascript/jsmin.c

    :Parameters:
      `python_only` : ``bool``
        Use only the python variant. If true, the c extension is not even
        tried to be loaded.

    :Return: Minifier
    :Rtype: ``callable``
    """
    # pylint: disable = unused-variable
    # pylint: disable = too-many-locals

    if not python_only:
        try:
            import _rjsmin
        except ImportError:
            pass
        else:
            return _rjsmin.jsmin
    try:
        xrange
    except NameError:
        xrange = range  # pylint: disable = redefined-builtin

    space_chars = r'[\000-\011\013\014\016-\040]'

    line_comment = r'(?://[^\r\n]*)'
    space_comment = r'(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/)'
    space_comment_nobang = r'(?:/\*(?!!)[^*]*\*+(?:[^/*][^*]*\*+)*/)'
    bang_comment = r'(?:/\*![^*]*\*+(?:[^/*][^*]*\*+)*/)'

    string1 = \
        r'(?:\047[^\047\\\r\n]*(?:\\(?:[^\r\n]|\r?\n|\r)[^\047\\\r\n]*)*\047)'
    string2 = r'(?:"[^"\\\r\n]*(?:\\(?:[^\r\n]|\r?\n|\r)[^"\\\r\n]*)*")'
    string3 = r'(?:`(?:[^`\\]|\\.)*`)'
    strings = r'(?:%s|%s|%s)' % (string1, string2, string3)

    charclass = r'(?:\[[^\\\]\r\n]*(?:\\[^\r\n][^\\\]\r\n]*)*\])'
    nospecial = r'[^/\\\[\r\n]'
    regex = r'(?:/(?![\r\n/*])%s*(?:(?:\\[^\r\n]|%s)%s*)*/)' % (
        nospecial, charclass, nospecial)
    space = r'(?:%s|%s)' % (space_chars, space_comment)
    newline = r'(?:%s?[\r\n])' % line_comment

    def fix_charclass(result):
        """ Fixup string of chars to fit into a regex char class """
        pos = result.find('-')
        if pos >= 0:
            result = r'%s%s-' % (result[:pos], result[pos + 1:])

        def sequentize(string):
            """
            Notate consecutive characters as sequence

            (1-4 instead of 1234)
            """
            first, last, result = None, None, []
            for char in map(ord, string):
                if last is None:
                    first = last = char
                elif last + 1 == char:
                    last = char
                else:
                    result.append((first, last))
                    first = last = char
            if last is not None:
                result.append((first, last))
            return ''.join([
                '%s%s%s' % (chr(first), last > first + 1 and '-'
                            or '', last != first and chr(last) or '')
                for first, last in result
            ])  # noqa

        return _re.sub(
            r'([\000-\040\047])',  # \047 for better portability
            lambda m: '\\%03o' % ord(m.group(1)),
            (sequentize(result).replace('\\', '\\\\').replace(
                '[', '\\[').replace(']', '\\]')))

    def id_literal_(what):
        """ Make id_literal like char class """
        match = _re.compile(what).match
        result = ''.join([chr(c) for c in xrange(127) if not match(chr(c))])
        return '[^%s]' % fix_charclass(result)

    def not_id_literal_(keep):
        """ Make negated id_literal like char class """
        match = _re.compile(id_literal_(keep)).match
        result = ''.join([chr(c) for c in xrange(127) if not match(chr(c))])
        return r'[%s]' % fix_charclass(result)

    not_id_literal = not_id_literal_(r'[a-zA-Z0-9_$]')
    preregex1 = r'[(,=:\[!&|?{};\r\n]'
    preregex2 = r'%(not_id_literal)sreturn' % locals()

    id_literal = id_literal_(r'[a-zA-Z0-9_$]')
    id_literal_open = id_literal_(r'[a-zA-Z0-9_${\[(!+-]')
    id_literal_close = id_literal_(r'[a-zA-Z0-9_$}\])"\047+-]')
    post_regex_off = id_literal_(r'[^\000-\040}\])?:|,;.&=+-]')

    dull = r'[^\047"`/\000-\040]'

    space_sub_simple = _re.compile((
        # noqa pylint: disable = bad-continuation
        r'(%(dull)s+)'  # 0
        r'|(%(strings)s%(dull)s*)'  # 1
        r'|(?<=%(preregex1)s)'
        r'%(space)s*(?:%(newline)s%(space)s*)*'
        r'(%(regex)s)'  # 2
        r'(%(space)s*(?:%(newline)s%(space)s*)+'  # 3
        r'(?=%(post_regex_off)s))?'
        r'|(?<=%(preregex2)s)'
        r'%(space)s*(?:(%(newline)s)%(space)s*)*'  # 4
        r'(%(regex)s)'  # 5
        r'(%(space)s*(?:%(newline)s%(space)s*)+'  # 6
        r'(?=%(post_regex_off)s))?'
        r'|(?<=%(id_literal_close)s)'
        r'%(space)s*(?:(%(newline)s)%(space)s*)+'  # 7
        r'(?=%(id_literal_open)s)'
        r'|(?<=%(id_literal)s)(%(space)s)+(?=%(id_literal)s)'  # 8
        r'|(?<=\+)(%(space)s)+(?=\+)'  # 9
        r'|(?<=-)(%(space)s)+(?=-)'  # 10
        r'|%(space)s+'
        r'|(?:%(newline)s%(space)s*)+') % locals()).sub

    # print space_sub_simple.__self__.pattern

    def space_subber_simple(match):
        """ Substitution callback """
        # pylint: disable = too-many-return-statements

        groups = match.groups()
        if groups[0]:
            return groups[0]
        elif groups[1]:
            return groups[1]
        elif groups[2]:
            if groups[3]:
                return groups[2] + '\n'
            return groups[2]
        elif groups[5]:
            return "%s%s%s" % (
                groups[4] and '\n' or '',
                groups[5],
                groups[6] and '\n' or '',
            )
        elif groups[7]:
            return '\n'
        elif groups[8] or groups[9] or groups[10]:
            return ' '
        else:
            return ''

    space_sub_banged = _re.compile((
        # noqa pylint: disable = bad-continuation
        r'(%(dull)s+)'  # 0
        r'|(%(strings)s%(dull)s*)'  # 1
        r'|(?<=%(preregex1)s)'
        r'(%(space)s*(?:%(newline)s%(space)s*)*)'  # 2
        r'(%(regex)s)'  # 3
        r'(%(space)s*(?:%(newline)s%(space)s*)+'  # 4
        r'(?=%(post_regex_off)s))?'
        r'|(?<=%(preregex2)s)'
        r'(%(space)s*(?:(%(newline)s)%(space)s*)*)'  # 5, 6
        r'(%(regex)s)'  # 7
        r'(%(space)s*(?:%(newline)s%(space)s*)+'  # 8
        r'(?=%(post_regex_off)s))?'
        r'|(?<=%(id_literal_close)s)'
        r'(%(space)s*(?:%(newline)s%(space)s*)+)'  # 9
        r'(?=%(id_literal_open)s)'
        r'|(?<=%(id_literal)s)(%(space)s+)(?=%(id_literal)s)'  # 10
        r'|(?<=\+)(%(space)s+)(?=\+)'  # 11
        r'|(?<=-)(%(space)s+)(?=-)'  # 12
        r'|(%(space)s+)'  # 13
        r'|((?:%(newline)s%(space)s*)+)'  # 14
    ) % locals()).sub

    # print space_sub_banged.__self__.pattern

    keep = _re.compile(
        (r'%(space_chars)s+|%(space_comment_nobang)s+|%(newline)s+'
         r'|(%(bang_comment)s+)') % locals()).sub
    keeper = lambda m: m.groups()[0] or ''

    # print keep.__self__.pattern

    def space_subber_banged(match):
        """ Substitution callback """
        # pylint: disable = too-many-return-statements

        groups = match.groups()
        if groups[0]:
            return groups[0]
        elif groups[1]:
            return groups[1]
        elif groups[3]:
            return "%s%s%s%s" % (
                keep(keeper, groups[2]),
                groups[3],
                keep(keeper, groups[4] or ''),
                groups[4] and '\n' or '',
            )
        elif groups[7]:
            return "%s%s%s%s%s" % (
                keep(keeper, groups[5]),
                groups[6] and '\n' or '',
                groups[7],
                keep(keeper, groups[8] or ''),
                groups[8] and '\n' or '',
            )
        elif groups[9]:
            return keep(keeper, groups[9]) + '\n'
        elif groups[10] or groups[11] or groups[12]:
            return keep(keeper, groups[10] or groups[11] or groups[12]) or ' '
        else:
            return keep(keeper, groups[13] or groups[14])

    def jsmin(script, keep_bang_comments=False):
        r"""
        Minify javascript based on `jsmin.c by Douglas Crockford`_\.

        Instead of parsing the stream char by char, it uses a regular
        expression approach which minifies the whole script with one big
        substitution regex.

        .. _jsmin.c by Douglas Crockford:
           http://www.crockford.com/javascript/jsmin.c

        :Parameters:
          `script` : ``str``
            Script to minify

          `keep_bang_comments` : ``bool``
            Keep comments starting with an exclamation mark? (``/*!...*/``)

        :Return: Minified script
        :Rtype: ``str``
        """
        # pylint: disable = redefined-outer-name

        if keep_bang_comments:
            return space_sub_banged(space_subber_banged,
                                    '\n%s\n' % script).strip()
        else:
            return space_sub_simple(space_subber_simple,
                                    '\n%s\n' % script).strip()

    return jsmin


jsmin = _make_jsmin()


def jsmin_for_posers(script, keep_bang_comments=False):
    r"""
    Minify javascript based on `jsmin.c by Douglas Crockford`_\.

    Instead of parsing the stream char by char, it uses a regular
    expression approach which minifies the whole script with one big
    substitution regex.

    .. _jsmin.c by Douglas Crockford:
       http://www.crockford.com/javascript/jsmin.c

    :Warning: This function is the digest of a _make_jsmin() call. It just
              utilizes the resulting regexes. It's here for fun and may
              vanish any time. Use the `jsmin` function instead.

    :Parameters:
      `script` : ``str``
        Script to minify

      `keep_bang_comments` : ``bool``
        Keep comments starting with an exclamation mark? (``/*!...*/``)

    :Return: Minified script
    :Rtype: ``str``
    """
    if not keep_bang_comments:
        rex = (
            r'([^\047"/\000-\040]+)|((?:(?:\047[^\047\\\r\n]*(?:\\(?:[^\r\n]'
            r'|\r?\n|\r)[^\047\\\r\n]*)*\047)|(?:"[^"\\\r\n]*(?:\\(?:[^\r\n]'
            r'|\r?\n|\r)[^"\\\r\n]*)*"))[^\047"/\000-\040]*)|(?<=[(,=:\[!&|?'
            r'{};\r\n])(?:[\000-\011\013\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*'
            r'][^*]*\*+)*/))*(?:(?:(?://[^\r\n]*)?[\r\n])(?:[\000-\011\013\0'
            r'14\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*)*((?:/(?![\r'
            r'\n/*])[^/\\\[\r\n]*(?:(?:\\[^\r\n]|(?:\[[^\\\]\r\n]*(?:\\[^\r'
            r'\n][^\\\]\r\n]*)*\]))[^/\\\[\r\n]*)*/))((?:[\000-\011\013\014'
            r'\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*(?:(?:(?://[^\r'
            r'\n]*)?[\r\n])(?:[\000-\011\013\014\016-\040]|(?:/\*[^*]*\*+(?:'
            r'[^/*][^*]*\*+)*/))*)+(?=[^\000-\040&)+,.:;=?\]|}-]))?|(?<=[\00'
            r'0-#%-,./:-@\[-^`{-~-]return)(?:[\000-\011\013\014\016-\040]|(?'
            r':/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*(?:((?:(?://[^\r\n]*)?[\r\n]'
            r'))(?:[\000-\011\013\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*'
            r'\*+)*/))*)*((?:/(?![\r\n/*])[^/\\\[\r\n]*(?:(?:\\[^\r\n]|(?:\['
            r'[^\\\]\r\n]*(?:\\[^\r\n][^\\\]\r\n]*)*\]))[^/\\\[\r\n]*)*/))(('
            r'?:[\000-\011\013\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)'
            r'*/))*(?:(?:(?://[^\r\n]*)?[\r\n])(?:[\000-\011\013\014\016-\04'
            r'0]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*)+(?=[^\000-\040&)+,.:;'
            r'=?\]|}-]))?|(?<=[^\000-!#%&(*,./:-@\[\\^`{|~])(?:[\000-\011\01'
            r'3\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*(?:((?:(?:'
            r'//[^\r\n]*)?[\r\n]))(?:[\000-\011\013\014\016-\040]|(?:/\*[^*]'
            r'*\*+(?:[^/*][^*]*\*+)*/))*)+(?=[^\000-\040"#%-\047)*,./:-@\\-^'
            r'`|-~])|(?<=[^\000-#%-,./:-@\[-^`{-~-])((?:[\000-\011\013\014\0'
            r'16-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/)))+(?=[^\000-#%-,./'
            r':-@\[-^`{-~-])|(?<=\+)((?:[\000-\011\013\014\016-\040]|(?:/\*['
            r'^*]*\*+(?:[^/*][^*]*\*+)*/)))+(?=\+)|(?<=-)((?:[\000-\011\013'
            r'\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/)))+(?=-)|(?:['
            r'\000-\011\013\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/)'
            r')+|(?:(?:(?://[^\r\n]*)?[\r\n])(?:[\000-\011\013\014\016-\040]'
            r'|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*)+')

        def subber(match):
            """ Substitution callback """
            groups = match.groups()
            return (groups[0] or groups[1]
                    or (groups[3] and (groups[2] + '\n')) or groups[2]
                    or (groups[5] and "%s%s%s" % (
                        groups[4] and '\n' or '',
                        groups[5],
                        groups[6] and '\n' or '',
                    )) or (groups[7] and '\n') or (groups[8] and ' ')
                    or (groups[9] and ' ') or (groups[10] and ' ') or '')
    else:
        rex = (
            r'([^\047"/\000-\040]+)|((?:(?:\047[^\047\\\r\n]*(?:\\(?:[^\r\n]'
            r'|\r?\n|\r)[^\047\\\r\n]*)*\047)|(?:"[^"\\\r\n]*(?:\\(?:[^\r\n]'
            r'|\r?\n|\r)[^"\\\r\n]*)*"))[^\047"/\000-\040]*)|(?<=[(,=:\[!&|?'
            r'{};\r\n])((?:[\000-\011\013\014\016-\040]|(?:/\*[^*]*\*+(?:[^/'
            r'*][^*]*\*+)*/))*(?:(?:(?://[^\r\n]*)?[\r\n])(?:[\000-\011\013'
            r'\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*)*)((?:/(?!'
            r'[\r\n/*])[^/\\\[\r\n]*(?:(?:\\[^\r\n]|(?:\[[^\\\]\r\n]*(?:\\[^'
            r'\r\n][^\\\]\r\n]*)*\]))[^/\\\[\r\n]*)*/))((?:[\000-\011\013\01'
            r'4\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*(?:(?:(?://[^'
            r'\r\n]*)?[\r\n])(?:[\000-\011\013\014\016-\040]|(?:/\*[^*]*\*+('
            r'?:[^/*][^*]*\*+)*/))*)+(?=[^\000-\040&)+,.:;=?\]|}-]))?|(?<=['
            r'\000-#%-,./:-@\[-^`{-~-]return)((?:[\000-\011\013\014\016-\040'
            r']|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*(?:((?:(?://[^\r\n]*)?['
            r'\r\n]))(?:[\000-\011\013\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*]['
            r'^*]*\*+)*/))*)*)((?:/(?![\r\n/*])[^/\\\[\r\n]*(?:(?:\\[^\r\n]|'
            r'(?:\[[^\\\]\r\n]*(?:\\[^\r\n][^\\\]\r\n]*)*\]))[^/\\\[\r\n]*)*'
            r'/))((?:[\000-\011\013\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]'
            r'*\*+)*/))*(?:(?:(?://[^\r\n]*)?[\r\n])(?:[\000-\011\013\014\01'
            r'6-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*)+(?=[^\000-\040&)'
            r'+,.:;=?\]|}-]))?|(?<=[^\000-!#%&(*,./:-@\[\\^`{|~])((?:[\000-'
            r'\011\013\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*(?:'
            r'(?:(?://[^\r\n]*)?[\r\n])(?:[\000-\011\013\014\016-\040]|(?:/'
            r'\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*)+)(?=[^\000-\040"#%-\047)*,./'
            r':-@\\-^`|-~])|(?<=[^\000-#%-,./:-@\[-^`{-~-])((?:[\000-\011\01'
            r'3\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))+)(?=[^\000'
            r'-#%-,./:-@\[-^`{-~-])|(?<=\+)((?:[\000-\011\013\014\016-\040]|'
            r'(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))+)(?=\+)|(?<=-)((?:[\000-\0'
            r'11\013\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))+)(?=-'
            r')|((?:[\000-\011\013\014\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*'
            r'\*+)*/))+)|((?:(?:(?://[^\r\n]*)?[\r\n])(?:[\000-\011\013\014'
            r'\016-\040]|(?:/\*[^*]*\*+(?:[^/*][^*]*\*+)*/))*)+)')

        keep = _re.compile(
            (r'[\000-\011\013\014\016-\040]+|(?:/\*(?!!)[^*]*\*+(?:[^/*][^*]*'
             r'\*+)*/)+|(?:(?://[^\r\n]*)?[\r\n])+|((?:/\*![^*]*\*+(?:[^/*][^'
             r'*]*\*+)*/)+)') % locals()).sub
        keeper = lambda m: m.groups()[0] or ''

        def subber(match):
            """ Substitution callback """
            groups = match.groups()
            return (groups[0] or groups[1] or (groups[3] and "%s%s%s%s" % (
                keep(keeper, groups[2]),
                groups[3],
                keep(keeper, groups[4] or ''),
                groups[4] and '\n' or '',
            )) or (groups[7] and "%s%s%s%s%s" % (
                keep(keeper, groups[5]),
                groups[6] and '\n' or '',
                groups[7],
                keep(keeper, groups[8] or ''),
                groups[8] and '\n' or '',
            )) or (groups[9] and keep(keeper, groups[9] + '\n'))
                    or (groups[10] and keep(keeper, groups[10]) or ' ')
                    or (groups[11] and keep(keeper, groups[11]) or ' ')
                    or (groups[12] and keep(keeper, groups[12]) or ' ')
                    or keep(keeper, groups[13] or groups[14]))

    return _re.sub(rex, subber, '\n%s\n' % script).strip()


if __name__ == '__main__':

    def main():
        """ Main """
        import sys as _sys

        argv = _sys.argv[1:]
        keep_bang_comments = '-b' in argv or '-bp' in argv or '-pb' in argv
        if '-p' in argv or '-bp' in argv or '-pb' in argv:
            xjsmin = _make_jsmin(python_only=True)
        else:
            xjsmin = jsmin

        _sys.stdout.write(
            xjsmin(_sys.stdin.read(), keep_bang_comments=keep_bang_comments))

    main()
