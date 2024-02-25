# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    script { name: "fetch.py" }
    unpack_archive: true
  }
  build {}
}

upload {
  universal: true
  pkg_prefix: "chromium/fuchsia"
}
