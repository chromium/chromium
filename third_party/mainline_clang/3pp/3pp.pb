create {
  source {
    url {
      download_url: "https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/llvm-r487747/clang-r487747.tar.gz"
      version: "clang-r487747"
      extension: ".gz"
    }
    patch_version: "cr1"
    unpack_archive: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}
