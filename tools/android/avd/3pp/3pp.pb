# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  platform_re: "linux-amd64"
  source {
    script {
      name: "fetch.py"
      # When this is true, artifacts will be directly fetched by calling
      # `fetch.py checkout <checkout arguments>`
      use_fetch_checkout_workflow: true
    }
    patch_version: "cr0"
  }
}

upload { pkg_prefix: "chromium/tools/android" }
