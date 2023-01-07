# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Fields for use when working with a physical linux device connected locally
linux_device_ip = "192.168.42.32"
linux_device_user = "potat"

fuchsia_device_ip = "192.168.42.64"

# The linux build directory.
linux_out_dir = "out/default"
# The fuchsia build directory.
fuchsia_out_dir = "out/fuchsia"
# The location in src that will store final statistical data on perftest results
results_dir = "results"
# The location in src that stores the information from each comparative
# invocation of a perftest
raw_linux_dir = results_dir + "/linux_raw"
raw_fuchsia_dir = results_dir + "/fuchsia_raw"

# A list of test targets to deploy to both devices. Stick to *_perftests.
test_targets = [
    "base:base_perftests",
    "net:net_perftests",
]
