create {
  platform_re: "linux-.*|mac-.*"
  source {
    url {
      download_url: "https://github.com/markmentovai/bootstrap_cmds/archive/ca2b82d5dee993aa8ed52a04cfa33a9d167618f9.tar.gz"
      version: "121.100.1"
    }
    unpack_archive: true
  }

  build {
    tool: "chromium/tools/flex"
  }
}

upload { pkg_prefix: "chromium/third_party" }
