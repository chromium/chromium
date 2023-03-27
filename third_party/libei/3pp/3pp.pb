create {
  platform_re: "linux-.*"
  source { script { name: "fetch.py" } }
  build {
    dep: "chromium/third_party/dbus"
    install: "install.sh"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
