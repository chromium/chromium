create {
  source {
    git {
      repo: "https://r8.googlesource.com/r8"
      tag_pattern: "%s-dev"
      version_restriction {
          op: EQ
          val: "3.1.16"
      }
    }
    patch_dir: "patches"
    patch_version: "crbug1214915"
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
