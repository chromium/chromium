create {
  platform_re: "linux-.*"
  source {
    url {
      download_url: "https://dbus.freedesktop.org/releases/dbus/dbus-1.14.0.tar.xz"
      version: "1.14.0"
    }
    unpack_archive: true
  }

  build {}
}

upload {
  pkg_prefix: "chromium/third_party"
}
