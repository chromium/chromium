create {
  platform_re: "linux-.*"
  source {
    url {
      download_url: "https://gitlab.freedesktop.org/pipewire/pipewire/-/archive/1.4.8/pipewire-1.4.8.tar.gz"
      version: "1.4.8"
    }
    unpack_archive: true
    patch_dir: "patches"
    patch_version: "chromium.1"
  }
  build {
    dep: "chromium/third_party/dbus"
    install: "install.sh"
    external_tool: "infra/3pp/tools/cpython3/${platform}@3@3.11.10.chromium.35"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
