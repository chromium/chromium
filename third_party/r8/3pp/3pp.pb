create {
  source {
    script { name: "fetch.py" }
    unpack_archive: true
    patch_dir: "patches"
  }

  build {
    dep: "chromium/third_party/jdk"
    # gradle cannot be executed correctly under docker env
    no_docker_env: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}
