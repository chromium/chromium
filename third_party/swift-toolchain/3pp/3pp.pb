# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(crbug.com/1340013): Have a linux section for the toolchain for 
# goma/reclient. It likely just needs to download the .tgz on swift.org and 
# unpack it.

create {
  platform_re: "mac-.*"
  source {
    url {
      download_url: "https://download.swift.org/swift-5.6.2-release/xcode/swift-5.6.2-RELEASE/swift-5.6.2-RELEASE-osx.pkg"
      version: "5.6.2"
    }
    # Needed to re-trigger packaging for 5.6.2. Not normally necessary.
    patch_version: "cr2"
  }
  build {
    install: "install-mac.sh"
    install: "swift-5.6.2-RELEASE-osx"
  }
}

upload {
  pkg_prefix: "chromium/tools"
  pkg_name_override: "swift-toolchain"
}
