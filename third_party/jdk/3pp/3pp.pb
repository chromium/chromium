# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  platform_re: "linux-amd64|mac-arm64"
  source {
    script {
      name: "3pp.py"
      use_fetch_checkout_workflow: true
    }
    patch_version: "cr0"
  }
  build {
    install: ["3pp.py", "install"]
  }
}

create {
  platform_re: "mac-arm64"
  build {
    # Only Linux builds a native-image (which requires a toolchain).
    no_toolchain: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
