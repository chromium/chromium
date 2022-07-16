create {
  source {
    script { name: "fetch.py" }
    unpack_archive: true
    no_archive_prune: true
  }
}

upload {
  universal: true
  pkg_prefix: "chromium/third_party/updater"
}
