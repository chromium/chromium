#!/usr/bin/env python3

# Copyright 2024 The Crashpad Authors
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
import platform
import subprocess
import sys


def main(args):
    """
    Executes the test-scripts with required environment variables. It acts like
    /usr/bin/env, but provides some extra functionality to dynamically set up
    the environment variables.

    Args:
        args: the command line arguments without the script name itself.
    """
    os.environ['SRC_ROOT'] = os.path.abspath(
        os.path.join(os.path.dirname(__file__), '..'))

    assert platform.system() == 'Linux', 'Unsupported OS ' + platform.system()
    os.environ['FUCHSIA_SDK_ROOT'] = os.path.join(
        os.environ['SRC_ROOT'], 'third_party/fuchsia/sdk/linux-amd64/')
    os.environ['FUCHSIA_GN_SDK_ROOT'] = os.path.join(
        os.environ['SRC_ROOT'], 'third_party/fuchsia-gn-sdk/src')

    return subprocess.run(args).returncode


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
