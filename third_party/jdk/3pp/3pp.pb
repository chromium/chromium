# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    url {
      # See the link "[Binaries]" in
      # https://wiki.openjdk.java.net/display/JDKUpdates/JDK11u#JDK11u-Releases
      download_url: "https://github.com/AdoptOpenJDK/openjdk11-upstream-binaries/releases/download/jdk-11.0.4%2B11/OpenJDK11U-jdk_x64_linux_11.0.4_11.tar.gz"
      version: "11.0.4+11"
    }
    patch_version: 'cr0'
    unpack_archive: true
    subdir: 'current'
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}
