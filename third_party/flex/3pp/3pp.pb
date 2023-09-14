create {
  platform_re: "linux-.*|mac-.*"
  source {
    url {
      # Downloading a release tarball removes the autconf and gettext dep.
      download_url: "https://github.com/westes/flex/releases/download/v2.6.4/flex-2.6.4.tar.gz"
      version: "2.6.4"
    }
    unpack_archive: true
    cpe_base_address: "cpe:/a:flex_project:flex"
  }

  build {}
}

upload { pkg_prefix: "chromium/tools" }
