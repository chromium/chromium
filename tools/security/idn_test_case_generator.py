#!/usr/bin/env python2
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for generating IDN test cases.

Either use the command-line interface (see --help) or directly call make_case
from Python shell (see make_case documentation).
"""

from __future__ import print_function

import argparse
import codecs
import doctest
import sys


def str_to_c_string(string):
    """Converts a Python str (ASCII) to a C string literal.

    >>> str_to_c_string('abc\x8c')
    '"abc\\\\x8c"'
    """
    return repr(string).replace("'", '"')


def ishexdigit(c):
    """
    >>> ishexdigit('0')
    True
    >>> ishexdigit('9')
    True
    >>> ishexdigit('/')
    False
    >>> ishexdigit(':')
    False
    >>> ishexdigit('a')
    True
    >>> ishexdigit('f')
    True
    >>> ishexdigit('g')
    False
    >>> ishexdigit('A')
    True
    >>> ishexdigit('F')
    True
    >>> ishexdigit('G')
    False
    """
    return c.isdigit() or ord('a') <= ord(c.lower()) <= ord('f')


def unicode_to_c_wstring(string):
    """Converts a Python str or unicode to a C wide-string literal.

    >>> unicode_to_c_wstring(u'b\u00fccher.de')
    'L"b\\\\x00fc" L"cher.de"'
    """
    result = ['L"']
    for c in string:
        # If the previous character was \x-escaped, and the next character is a
        # hex digit, we need to end and restart the string literal. Otherwise,
        # the next character will extend the \x escape sequence.
        if result[-1].startswith('\\x') and ishexdigit(c):
            result.append('" L"')
        escaped = repr(c)[2:-1]
        # Convert '\u' to '\x', and also force a minimum of 4 digits (this isn't
        # necessary but is preferred style for these test cases).
        if escaped[:2] in ('\\x', '\\u'):
            escaped = '\\x%04x' % ord(c)
        result.append(escaped)
    result.append('"')
    return ''.join(result)


def make_case(unicode_domain, unicode_allowed=True, case_name=None):
    """Generates a C++ test case for an IDN domain test.

    This is designed specifically for the IDNTestCase struct in the file
    components/url_formatter/url_formatter_unittest.cc. It generates a row of
    the idn_cases array, specifying a test for a particular domain.

    |unicode_domain| is a Unicode string of the domain (NOT IDNA-encoded).
    |unicode_allowed| specifies whether the test case should expect the domain
    to be displayed in Unicode form (True) or in IDNA/Punycode ASCII encoding
    (False). |case_name| is just for the comment.

    This function will automatically convert the domain to its IDNA format, and
    prepare the test case in C++ syntax.

    >>> make_case(u'\u5317\u4eac\u5927\u5b78.cn', True, 'Hanzi (Chinese)')
        // Hanzi (Chinese)
        {"xn--1lq90ic7f1rc.cn", L"\\x5317\\x4eac\\x5927\\x5b78.cn", true},
    >>> make_case(u'b\u00fccher.de', True)
        {"xn--bcher-kva.de", L"b\\x00fc" L"cher.de", true},

    This will also apply normalization to the Unicode domain, as required by the
    IDNA algorithm. This example shows U+210F normalized to U+0127 (this
    generates the exact same test case as u'\u0127ello'):

    >>> make_case(u'\u210fello', True)
        {"xn--ello-4xa", L"\\x0127" L"ello", true},
    """
    idna_input = codecs.encode(unicode_domain, 'idna')
    # Round-trip to ensure normalization.
    unicode_output = codecs.decode(idna_input, 'idna')
    if case_name:
      print('    // %s' % case_name)
    print('    {%s, %s, %s},' %
          (str_to_c_string(idna_input), unicode_to_c_wstring(unicode_output),
           repr(unicode_allowed).lower()))


def main(args=None):
    if args is None:
        args = sys.argv[1:]

    parser = argparse.ArgumentParser(description='Generate an IDN test case.')
    parser.add_argument('domain', metavar='DOMAIN', nargs='?',
                        help='the Unicode domain (not encoded)')
    parser.add_argument('--name', metavar='NAME',
                        help='the name of the test case')
    parser.add_argument('--no-unicode', action='store_false',
                        dest='unicode_allowed', default=True,
                        help='expect the domain to be Punycoded')
    parser.add_argument('--test', action='store_true', dest='run_tests',
                        help='run unit tests')

    args = parser.parse_args(args)

    if args.run_tests:
        import doctest
        doctest.testmod()
        return

    if not args.domain:
        parser.error('Required argument: DOMAIN')

    # Assume stdin.encoding is the encoding used for command-line arguments.
    domain = args.domain.decode(sys.stdin.encoding)
    make_case(domain, unicode_allowed=args.unicode_allowed, case_name=args.name)


if __name__ == '__main__':
    sys.exit(main())
