# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    url {
      # See: https://maven.apache.org/download.cgi
      download_url: "https://archive.apache.org/dist/maven/maven-3/3.9.15/binaries/apache-maven-3.9.15-bin.tar.gz"
      version: "3.9.15"
    }
    unpack_archive: true
    cpe_base_address: "cpe:/a:apache:maven"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}
