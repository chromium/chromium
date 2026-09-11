# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  platform_re: "linux-amd64|mac-arm64"
  source {
    script { name: "fetch.py" }
    subdir: "lib"
  }
  build {
    install: "install.py"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
