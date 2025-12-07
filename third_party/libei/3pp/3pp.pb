create {
  platform_re: "linux-.*"
  source { script { name: "fetch.py" } }
  build {
    install: "install.sh"
    external_tool: "infra/3pp/tools/cpython3/${platform}@3@3.11.10.chromium.35"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
