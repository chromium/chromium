# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Platform-specific utilities and pseudo-constants

Any functions whose implementations or values differ from one platform to
another should be defined in their respective platform_utils_<platform>.py
modules. The appropriate one of those will be imported into this module to
provide callers with a common, platform-independent interface.
"""

import sys

# We may not support the version of Python that a user has installed (Cygwin
# especially has had problems), but we'll allow the platform utils to be
# included in any case so we don't get an import error.
if sys.platform in ('cygwin', 'win32'):
  from platform_utils_win import *
elif sys.platform == 'darwin':
  from platform_utils_mac import *
elif sys.platform.startswith('linux'):
  from platform_utils_linux import *
