create {
  platform_re: "windows-amd64"
  source {
    script { name: "fetch.py" }
    unpack_archive: true
    subdir: "third_party/win_virtual_display"
  }
  build {
    install: "build.py"
    no_toolchain: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
