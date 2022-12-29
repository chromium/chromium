# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    script { name: "fetch.py" }
  }

  build {
    install: "install.sh"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}
