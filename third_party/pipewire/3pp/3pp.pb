create {
  platform_re: "linux-.*"
  source {
    url {
      download_url: "https://gitlab.freedesktop.org/pipewire/pipewire/-/archive/0.3.56/pipewire-0.3.56.tar.gz"
      version: "0.3.56"
    }
    unpack_archive: true
  }

  build {
    dep: "chromium/third_party/dbus"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
