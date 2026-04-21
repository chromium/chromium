create {
  source {
    script { name: "fetch.py" }
    unpack_archive: true
  }

  build {
    install: "install.sh"
  }
}

upload {
  universal: true
  pkg_prefix: "chromium/third_party"
}