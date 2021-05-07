create {
  platform_re: "linux-.*|mac-.*"
  source {
    url {
      download_url: "https://github.com/markmentovai/bootstrap_cmds/archive/1146f2bf0e78b3a4855fb556cc08d83568d28a4a.tar.gz"
      version: "121"
    }
    unpack_archive: true
  }

  build {
    tool: "chromium/tools/flex"
  }
}

upload { pkg_prefix: "chromium/third_party" }
