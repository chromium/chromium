# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The GN arg `strip_absolute_paths_from_debug_symbols = 1` uses relative paths
# for debug symbols. This confuses lldb. We explicitly set the source-map to
# point at the root directory of the chromium checkout.
import os
import lldb
this_dir = os.path.dirname(os.path.abspath(__file__))
source_dir = os.path.join(os.path.join(this_dir, os.pardir), os.pardir)

lldb.debugger.HandleCommand(
    'settings set target.source-map ../.. ' + source_dir)
lldb.debugger.HandleCommand(
    'settings set target.env-vars CHROMIUM_LLDBINIT_SOURCED=1')
