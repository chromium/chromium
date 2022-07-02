create {
  source {
    script { name: "fetch.py" }
  }
  build {
  }
}

upload {
  universal: true
  pkg_prefix: "chromium/third_party/updater"
}
