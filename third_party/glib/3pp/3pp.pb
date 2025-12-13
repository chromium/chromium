create {
  platform_re: "linux-.*"
  source {
    url {
      download_url: "https://download.gnome.org/sources/glib/2.82/glib-2.82.1.tar.xz"
      version: "2.82.1"
    }
    unpack_archive: true
    cpe_base_address: "cpe:/a:gnome:glib"
  }
  build {
    install: "install.sh"
    external_tool: "infra/3pp/tools/cpython3/${platform}@3@3.11.10.chromium.35"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
