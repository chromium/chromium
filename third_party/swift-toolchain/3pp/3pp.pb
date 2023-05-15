# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(crbug.com/1340013): Have a linux section for the toolchain for
# goma/reclient. It likely just needs to download the .tgz on swift.org and
# unpack it.

# IMPORTANT: When changing the download URL, also be sure to update the install
# directory in the `build` section. Otherwise the packaging bot will fail.

# IMPORTANT: When updating the version, you need to also update he version
# in //build/toolchain/apple/toolchain.gni so that all .swift modules are
# correctly considered dirty and built with the new version of the compiler.

create {
  platform_re: "mac-.*"
  source {
    url {
      download_url: "https://download.swift.org/swift-5.8-release/xcode/swift-5.8-RELEASE/swift-5.8-RELEASE-osx.pkg"
      version: "5.8-release"
    }
  }
  build {
    install: "install-mac.sh"
    install: "swift-5.8-RELEASE-osx"
  }
}

upload {
  pkg_prefix: "chromium/tools"
  pkg_name_override: "swift-toolchain"
}
