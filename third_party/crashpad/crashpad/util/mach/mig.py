#!/usr/bin/env python

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

import os
import shutil
import sys
import tempfile

import mig_fix
import mig_gen


def _try_remove(*paths):
    for path in paths:
        try:
            os.remove(path)
        except OSError:
            pass


def _generate_and_fix(user_c, server_c, user_h, server_h, defs, include, sdk,
                      clang_path, mig_path, migcom_path, arch):
    interface = mig_gen.MigInterface(user_c, server_c, user_h, server_h)
    mig_gen.generate_interface(defs, interface, include, sdk, clang_path,
                               mig_path, migcom_path, arch)
    mig_fix.fix_interface(interface)


def _wrap_arch_guards(file, arch):
    contents = '#if defined(__%s__)\n' % arch
    contents += open(file, 'r').read()
    contents += '\n#endif  /* __%s__ */\n' % arch
    return contents


def _write_file(path, data):
    with open(path, 'w') as file:
        file.write(data)


def main(args):
    parsed = mig_gen.parse_args(args, multiple_arch=True)

    _try_remove(parsed.user_c, parsed.server_c, parsed.user_h, parsed.server_h)

    if len(parsed.arch) <= 1:
        _generate_and_fix(parsed.user_c, parsed.server_c, parsed.user_h,
                          parsed.server_h, parsed.defs, parsed.include,
                          parsed.sdk, parsed.clang_path, parsed.mig_path,
                          parsed.migcom_path,
                          parsed.arch[0] if len(parsed.arch) >= 1 else None)
        return 0

    # Run mig once per architecture, and smush everything together, wrapped in
    # in architecture-specific #if guards.

    user_c_data = ''
    server_c_data = ''
    user_h_data = ''
    server_h_data = ''

    for arch in parsed.arch:
        # Python 3: use tempfile.TempDirectory instead
        temp_dir = tempfile.mkdtemp(prefix=os.path.basename(sys.argv[0]) + '_')
        try:
            user_c = os.path.join(temp_dir, os.path.basename(parsed.user_c))
            server_c = os.path.join(temp_dir, os.path.basename(parsed.server_c))
            user_h = os.path.join(temp_dir, os.path.basename(parsed.user_h))
            server_h = os.path.join(temp_dir, os.path.basename(parsed.server_h))
            _generate_and_fix(user_c, server_c, user_h, server_h, parsed.defs,
                              parsed.include, parsed.sdk, parsed.clang_path,
                              parsed.mig_path, parsed.migcom_path, arch)

            user_c_data += _wrap_arch_guards(user_c, arch)
            server_c_data += _wrap_arch_guards(server_c, arch)
            user_h_data += _wrap_arch_guards(user_h, arch)
            server_h_data += _wrap_arch_guards(server_h, arch)
        finally:
            shutil.rmtree(temp_dir)

    _write_file(parsed.user_c, user_c_data)
    _write_file(parsed.server_c, server_c_data)
    _write_file(parsed.user_h, user_h_data)
    _write_file(parsed.server_h, server_h_data)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
