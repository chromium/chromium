create {
  platform_re: "linux-.*"
  source {
    url {
      download_url: "https://gitlab.freedesktop.org/pipewire/wireplumber/-/archive/0.5.11/wireplumber-0.5.11.tar.bz2"
      version: "0.5.11"
    }
    unpack_archive: true
    patch_dir: "patches"
    patch_version: "chromium.1"
  }

  build {
    dep: "chromium/third_party/glib"
    dep: "chromium/third_party/pipewire"
    install: "install.sh"
    external_tool: "infra/3pp/tools/cpython3/${platform}@3@3.11.10.chromium.35"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
