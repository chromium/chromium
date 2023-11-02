create {
  platform_re: "linux-.*"
  source {
    url {
      download_url: "https://gitlab.freedesktop.org/pipewire/media-session/-/archive/0.4.1/media-session-0.4.1.tar.gz"
      version: "0.4.1"
    }
    unpack_archive: true
  }

  build {
    dep: "chromium/third_party/alsa",
    dep: "chromium/third_party/dbus",
    dep: "chromium/third_party/pipewire"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
