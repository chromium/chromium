#!/usr/bin/env python
# coding: utf-8

# Copyright 2019 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import os
import re
import sys

from mig_gen import MigInterface


def _fix_user_implementation(implementation, fixed_implementation, header,
                             fixed_header):
    """Rewrites a MIG-generated user implementation (.c) file.

    Rewrites the file at |implementation| by adding “__attribute__((unused))” to
    the definition of any structure typedefed as “__Reply” by searching for the
    pattern unique to those structure definitions. These structures are in fact
    unused in the user implementation file, and this will trigger a
    -Wunused-local-typedefs warning in gcc unless removed or marked with the
    “unused” attribute. Also changes header references to point to the new
    header filename, if changed.

    If |fixed_implementation| is None, overwrites the original; otherwise, puts
    the result in the file at |fixed_implementation|.
    """

    file = open(implementation, 'r+' if fixed_implementation is None else 'r')
    contents = file.read()

    pattern = re.compile('^(\t} __Reply);$', re.MULTILINE)
    contents = pattern.sub(r'\1 __attribute__((unused));', contents)

    if fixed_header is not None:
        contents = contents.replace(
            '#include "%s"' % os.path.basename(header),
            '#include "%s"' % os.path.basename(fixed_header))

    if fixed_implementation is None:
        file.seek(0)
        file.truncate()
    else:
        file.close()
        file = open(fixed_implementation, 'w')
    file.write(contents)
    file.close()


def _fix_server_implementation(implementation, fixed_implementation, header,
                               fixed_header):
    """Rewrites a MIG-generated server implementation (.c) file.

    Rewrites the file at |implementation| by replacing “mig_internal” with
    “mig_external” on functions that begin with “__MIG_check__”. This makes
    these functions available to other callers outside this file from a linkage
    perspective. It then returns, as a list of lines, declarations that can be
    added to a header file, so that other files that include that header file
    will have access to these declarations from a compilation perspective. Also
    changes header references to point to the new header filename, if changed.

    If |fixed_implementation| is None or not provided, overwrites the original;
    otherwise, puts the result in the file at |fixed_implementation|.
    """

    file = open(implementation, 'r+' if fixed_implementation is None else 'r')
    contents = file.read()

    # Find interesting declarations.
    declaration_pattern = re.compile(
        '^mig_internal (kern_return_t __MIG_check__.*)$', re.MULTILINE)
    declarations = declaration_pattern.findall(contents)

    # Remove “__attribute__((__unused__))” from the declarations, and call them
    # “mig_external” or “extern” depending on whether “mig_external” is defined.
    attribute_pattern = re.compile(r'__attribute__\(\(__unused__\)\) ')
    declarations = [
        '''\
#ifdef mig_external
mig_external
#else
extern
#endif
''' + attribute_pattern.sub('', x) + ';\n' for x in declarations
    ]

    # Rewrite the declarations in this file as “mig_external”.
    contents = declaration_pattern.sub(r'mig_external \1', contents)

    # Crashpad never implements the mach_msg_server() MIG callouts. To avoid
    # needing to provide stub implementations, set KERN_FAILURE as the RetCode
    # and abort().
    routine_callout_pattern = re.compile(
        r'OutP->RetCode = (([a-zA-Z0-9_]+)\(.+\));')
    routine_callouts = routine_callout_pattern.findall(contents)
    for routine in routine_callouts:
        contents = contents.replace(routine[0], 'KERN_FAILURE; abort()')

    # Include the header for abort().
    contents = '#include <stdlib.h>\n' + contents

    if fixed_header is not None:
        contents = contents.replace(
            '#include "%s"' % os.path.basename(header),
            '#include "%s"' % os.path.basename(fixed_header))

    if fixed_implementation is None:
        file.seek(0)
        file.truncate()
    else:
        file.close()
        file = open(fixed_implementation, 'w')
    file.write(contents)
    file.close()
    return declarations


def _fix_header(header, fixed_header, declarations=[]):
    """Rewrites a MIG-generated header (.h) file.

    Rewrites the file at |header| by placing it inside an “extern "C"” block, so
    that it declares things properly when included by a C++ compilation unit.
    |declarations| can be a list of additional declarations to place inside the
    “extern "C"” block after the original contents of |header|.

    If |fixed_header| is None or not provided, overwrites the original;
    otherwise, puts the result in the file at |fixed_header|.
    """

    file = open(header, 'r+' if fixed_header is None else 'r')
    contents = file.read()
    declarations_text = ''.join(declarations)
    contents = '''\
#ifdef __cplusplus
extern "C" {
#endif

%s
%s
#ifdef __cplusplus
}
#endif
''' % (contents, declarations_text)

    if fixed_header is None:
        file.seek(0)
        file.truncate()
    else:
        file.close()
        file = open(fixed_header, 'w')
    file.write(contents)
    file.close()


def fix_interface(interface, fixed_interface=None):
    if fixed_interface is None:
        fixed_interface = MigInterface(None, None, None, None)

    _fix_user_implementation(interface.user_c, fixed_interface.user_c,
                             interface.user_h, fixed_interface.user_h)
    server_declarations = _fix_server_implementation(interface.server_c,
                                                     fixed_interface.server_c,
                                                     interface.server_h,
                                                     fixed_interface.server_h)
    _fix_header(interface.user_h, fixed_interface.user_h)
    _fix_header(interface.server_h, fixed_interface.server_h,
                server_declarations)


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('user_c')
    parser.add_argument('--fixed_user_c', default=None)
    parser.add_argument('server_c')
    parser.add_argument('--fixed_server_c', default=None)
    parser.add_argument('user_h')
    parser.add_argument('--fixed_user_h', default=None)
    parser.add_argument('server_h')
    parser.add_argument('--fixed_server_h', default=None)
    parsed = parser.parse_args(args)

    interface = MigInterface(parsed.user_c, parsed.server_c, parsed.user_h,
                             parsed.server_h)
    fixed_interface = MigInterface(parsed.fixed_user_c, parsed.fixed_server_c,
                                   parsed.fixed_user_h, parsed.fixed_server_h)
    fix_interface(interface, fixed_interface)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
