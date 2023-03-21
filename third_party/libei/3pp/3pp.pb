create {
  platform_re: "linux-.*"
  source {
    url {
      download_url: "https://gitlab.freedesktop.org/libinput/libei/-/archive/0.4.1/libei-0.4.1.tar.gz"
      version: "0.4.1"
    }
    unpack_archive: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
