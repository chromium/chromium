create {
  source {
    script { name: "fetch.py" }
    unpack_archive: true
    patch_version: "cr1"
  }

  build {
    tool: "chromium/third_party/maven"
    dep: "chromium/third_party/jdk"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}
