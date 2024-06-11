# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    script { name: "fetch.py" }
    unpack_archive: true
    subdir: 'current'
    patch_version: "cr4"
  }

  # This will execute the `install.sh` script in 3pp dir after the source CIPD
  # package is pulled.
  build {
    install: "install.sh"
    no_toolchain: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}
