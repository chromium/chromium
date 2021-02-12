# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    url {
      # See the link "[Binaries]" in
      # https://wiki.openjdk.java.net/display/jdk8u#Main-Releases
      download_url: "https://github.com/AdoptOpenJDK/openjdk8-upstream-binaries/releases/download/jdk8u275-b01/OpenJDK8U-jdk_x64_linux_8u275b01.tar.gz"
      version: "8u275-b01"
    }
    patch_version: 'cr0'
    unpack_archive: true
  }
  build {}
}

upload {
  pkg_prefix: "chromium/third_party/jdk"
  universal: true
}
