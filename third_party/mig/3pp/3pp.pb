create {
  platform_re: "linux-.*|mac-.*"
  source {
    url {
      download_url: "https://github.com/markmentovai/bootstrap_cmds/archive/3cc6b1cf291f8fccfbf6444d6630a02a54c16831.tar.gz"
      version: "122"
    }
    unpack_archive: true
  }

  build {
    tool: "chromium/tools/flex"
  }
}

upload { pkg_prefix: "chromium/third_party" }
