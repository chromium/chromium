# Copyright 2019 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import io
import unittest

from json5.host import Host

class HostTest(unittest.TestCase):
    maxDiff = None

    def test_directory_and_file_operations(self):
        h = Host()
        orig_cwd = h.getcwd()

        try:
            d = h.mkdtemp()
            h.chdir(d)
            h.write_text_file('foo', 'bar')
            contents = h.read_text_file('foo')
            self.assertEqual(contents, 'bar')
            h.chdir('..')
            h.rmtree(d)
        finally:
            h.chdir(orig_cwd)

    def test_print(self):
        s = io.StringIO()
        h = Host()
        h.print_('hello, world', stream=s)
        self.assertEqual('hello, world\n', s.getvalue())


if __name__ == '__main__':  # pragma: no cover
    unittest.main()
