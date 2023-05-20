create {
  source {
    git {
      repo: "https://github.com/jacoco/jacoco.git"
      tag_pattern: "v%s"
      version_restriction {
        op: EQ
        val: "0.8.8"
      }
    }
    patch_dir: "patches"
    patch_version: "chromium.1"
  }
  build {
    install: "install.py"
    tool: "chromium/third_party/maven"
    dep: "chromium/third_party/jdk"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}