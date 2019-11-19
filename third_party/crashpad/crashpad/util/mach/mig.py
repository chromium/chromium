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

import sys

import mig_fix
import mig_gen

def main(args):
    parsed = mig_gen.parse_args(args)

    interface = mig_gen.MigInterface(parsed.user_c, parsed.server_c,
                                     parsed.user_h, parsed.server_h)
    mig_gen.generate_interface(parsed.defs, interface, parsed.include,
                               parsed.sdk, parsed.clang_path, parsed.mig_path,
                               parsed.migcom_path)
    mig_fix.fix_interface(interface)

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
