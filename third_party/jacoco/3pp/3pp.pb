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
    patch_version: "chromium.4.2"
  }
  build {
    install: "install.py"
    tool: "chromium/third_party/maven"
    # Pin to a JDK version that's known to work.
    external_dep: "chromium/third_party/jdk@2@jdk-17.0.6+10.f601e9c320"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}