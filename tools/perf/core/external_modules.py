# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Allow importing external modules which may be missing in some platforms.

These modules are normally provided by the vpython environment manager. But
some platforms, e.g. CromeOs, do not have access to this facility.

To be safe, instead of e.g.:

    import pandas

clients should do:

    from core.external_modules import pandas

Tests that require pandas to work can be skipped as follows:

    from core.external_modules import pandas


    @unittest.skipIf(pandas is None, 'pandas not available')
    class TestsForMyModule(unittest.TestCase):
      def testSomeBehavior(self):
        # test some behavior that requires pandas module.

Finally, scripts that to work properly require any of these external
dependencies should call:

    from core import external_modules

    if __name__ == '__main__':
      external_modules.RequireModules()
      # the rest of your script here.

to exit early with a suitable error message if the dependencies are not
satisfied.
"""

import sys

try:
  import numpy  # pylint: disable=import-error
except ImportError:
  numpy = None

try:
  import pandas  # pylint: disable=import-error
except ImportError:
  pandas = None


def RequireModules():
  if numpy is None or pandas is None:
    sys.exit(
        'ERROR: Some required python modules are not available.\n\n'
        'Make sure to run this script using vpython or ensure that '
        'module dependencies listed in src/.vpython are satisfied.')
