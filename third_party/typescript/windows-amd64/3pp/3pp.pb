# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# NOTE: Before updating versions below run the follow from the repo root:
#
# ./third_party/node/linux/node-linux-x64/bin/node third_party/typescript/check_soak_time.js
#
# to find the latest version that has soaked for at least 3 weeks.
# DO NOT USE versions that don't meet the soak time requirement.

create {
  source {
    url {
      download_url: "https://registry.npmjs.org/@typescript/typescript-win32-x64/-/typescript-win32-x64-7.0.1-rc.tgz"
      version: "7.0.1-rc"
      extension: ".tgz"
    }
    unpack_archive: true
    patch_dir: "patches"
  }
  build {
    install: "install.sh"
  }
}

upload {
  pkg_prefix: "chromium/third_party/typescript"
  universal: true
}
