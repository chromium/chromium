# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    url {
      # See: https://maven.apache.org/download.cgi
      download_url: "https://downloads.apache.org/maven/maven-3/3.8.6/binaries/apache-maven-3.8.6-bin.tar.gz"
      version: "3.8.6"
    }
    unpack_archive: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}
