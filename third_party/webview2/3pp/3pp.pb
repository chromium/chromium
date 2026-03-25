create {
  source {
    script {
      name: "fetch.py"
    }
    unpack_archive: true

    # Adding a `patch_version` forces a re-run of `install.sh`, which is needed
    # to pick up r1604911.
    patch_version: "r1"
  }

  build {
    install: "install.sh"
  }
}

upload {
  universal: true
  pkg_prefix: "chromium/third_party"
}
