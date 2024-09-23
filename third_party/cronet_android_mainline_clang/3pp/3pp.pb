create {
  source {
    url {
      download_url: "https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/main/clang-r522817.tar.gz"
      version: "clang-r522817"
      extension: ".gz"
    }
    patch_version: "cr1"
    unpack_archive: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
