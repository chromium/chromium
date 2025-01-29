# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  platform_re: "linux-amd64|mac-arm64"
  source {
    script { name: "fetch.py" }
    unpack_archive: true
    # Since the structure on after unpack is like:
    #  - linux: "jdk-23.0.2+7/{bin,conf,...}"
    #  - mac: "jdk-23.0.2+7/Contents/{Home,MacOS,...}"
    # The default pruning process on mac will trim the "jdk-23.0.2+7/Contents",
    # but we want to keep "Contents" since it's the standard structure.
    # So we set "no_archive_prune" to true, for all platforms,
    # and prune the "jdk-23.0.2+7" in the install script.
    no_archive_prune: true
  }
}

# This will execute the `install.sh` script in 3pp dir after the source CIPD
# package is pulled.
create {
  platform_re: "linux-amd64"
  source {
    patch_version: "cr0"
  }
  build {
    install: "install.sh"
    no_toolchain: true
  }
}
create {
  platform_re: "mac-arm64"
  source {
    patch_version: "cr0"
  }
  build {
    install: "install_mac.sh"
    no_toolchain: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
