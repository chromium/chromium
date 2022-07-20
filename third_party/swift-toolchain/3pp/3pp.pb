# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(crbug.com/1340013): Have a linux section for the toolchain for 
# goma/reclient. It likely just needs to download the .tgz on swift.org and 
# unpack it.

# IMPORTANT: When changing the download URL, also be sure to update the install
# directory in the `build` section. Otherwise the packaging bot will fail.

create {
  platform_re: "mac-.*"
  source {
    url {
      download_url: "https://download.swift.org/swift-5.7-branch/xcode/swift-5.7-DEVELOPMENT-SNAPSHOT-2022-07-17-a/swift-5.7-DEVELOPMENT-SNAPSHOT-2022-07-17-a-osx.pkg"
      version: "5.7-20220717"
    }
  }
  build {
    install: "install-mac.sh"
    install: "swift-5.7-DEVELOPMENT-SNAPSHOT-2022-07-17-a-osx"
  }
}

upload {
  pkg_prefix: "chromium/tools"
  pkg_name_override: "swift-toolchain"
}
