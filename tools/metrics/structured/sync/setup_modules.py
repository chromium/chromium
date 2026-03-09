# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# AUTO GENERATED, DO NOT EDIT MANUALLY.
# SEE: tools/metrics/generate_setup_modules.py
#
# This is a shim allowing universal setup of tools/metrics scripts.
# It calls setup_modules_lib.setup_modules() to create a common
# environment whenever imported.
#
# Each subdirectory in tools/metrics includes a setup_modules.py
# file which should be imported at the top of each script, like so:
#
# ```
# import setup_modules
#
# # Example import afterwards: Importing src/tools/metrics/common/models.py
# import chrome_src.tools.metrics.common.models as models
# ```

from pathlib import Path

import os
import sys

_CHROMIUM_SRC_RELATIVE_PATH = '../../../..'

base_path = Path(os.path.dirname(os.path.abspath(__file__)))

# Add src/tools/metrics to path temporarily to import the setup_modules_lib.
chromium_src_path = base_path.joinpath(_CHROMIUM_SRC_RELATIVE_PATH).resolve()
setup_modules_path = chromium_src_path.joinpath('tools', 'metrics',
                                                'python_support').resolve()

sys.path.append(str(setup_modules_path))
import setup_modules_lib

# Restore the path to an extent that it's possible, so
# that it doesn't interfere with actual modules setup.
sys.path.remove(str(setup_modules_path))

# Actually set up the modules using setup_modules_lib.
setup_modules_lib.setup_modules(str(chromium_src_path))
