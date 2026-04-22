# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    url {
      # See: https://maven.apache.org/download.cgi
      download_url: "https://downloads.apache.org/maven/maven-3/3.9.14/binaries/apache-maven-3.9.14-bin.tar.gz"
      version: "3.9.14"
    }
    unpack_archive: true
    cpe_base_address: "cpe:/a:apache:maven"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}
