# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    url {
      # See the link "[Binaries]" in
      # https://wiki.openjdk.java.net/display/JDKUpdates/JDK11u#JDK11u-Releases
      download_url: "https://github.com/adoptium/temurin11-binaries/releases/download/jdk-11.0.15%2B10/OpenJDK11U-jdk_x64_linux_hotspot_11.0.15_10.tar.gz"
      version: "11.0.15+10"
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
