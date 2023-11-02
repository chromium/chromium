#!/usr/bin/env python
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates the list of locales with strings for the current platform.

This program generates a file which solely contains a list of locales that will
be built for this platform in this build of Chrome (equivalently the locales
that we build strings for) passed in as arguments to this script. This allows us
to determine what locales we have strings for at runtime without checking the
locale data paks on the filesystem with a blocking I/O call.

This program can be run with no arguments to run its own unit tests. The first
argument is the output filename, and all later arguments are interpreted as
locales.

This script is run by the //ui/base:locales_list_gen build rule, passing in the
gn variable `locales` defined in build/config/locales.gni as arguments.
The generated file is then included in ui/base/l10n/l10n_util.h.
"""

from __future__ import print_function

import sys


def gen_locale(locale):  # type: (str) -> str
    """Returns the generated code for a given locale in the list."""
    # We assume that all locale codes have only letters, numbers and hyphens.
    assert locale.replace('-', '').isalnum(), locale
    # clang-format enforces a four-space indent for initializer lists.
    return '    PLATFORM_LOCALE({locale})'.format(locale=locale)


def gen_locales(locales):  # type: (list) -> str
    """Returns the generated code for the locale list.

    The list is guaranteed to be in sorted order without duplicates.

    >>> locales = ['en-GB', 'en', 'de', 'en']
    >>> generated = gen_locales(locales)
    >>> locales.pop()  # remove the duplicate
    'en'
    >>> locales.sort()
    >>> index_in_generated = lambda locale: generated.index(gen_locale(locale))
    >>> all(
    ...     index_in_generated(prev_locale) < index_in_generated(next_locale)
    ...     for prev_locale, next_locale in zip(locales, locales[1:]))
    True
    """
    return '\n'.join(gen_locale(locale) for locale in sorted(set(locales)))


def main():  # type: () -> None
    import doctest
    doctest.testmod()

    if len(sys.argv) < 2:
        print('{}: only ran tests'.format(sys.argv[0]))
        return

    output = gen_locales(sys.argv[2:])
    with open(sys.argv[1], 'w') as f:
        f.write(output)


if __name__ == '__main__':
    main()
