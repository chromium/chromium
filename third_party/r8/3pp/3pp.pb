# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    script {
      name: "3pp.py"
      use_fetch_checkout_workflow: true
    }
  }

  build {
    install: ["3pp.py", "install"]
    # gradle cannot be executed correctly under docker env
    no_docker_env: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}
