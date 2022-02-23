create {
  source {
    script { name: "fetch.py" }
    unpack_archive: true
    no_archive_prune: true
  }
  build {
  }
}

upload {
  universal: true
  pkg_prefix: "chromium/third_party/updater"
}
