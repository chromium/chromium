create {
  source {
    git {
      repo: "https://github.com/jacoco/jacoco.git"
      tag_pattern: "v%s"
      version_restriction {
        op: EQ
        val: "0.8.3"
      }
    }
    patch_dir: "patches"
    patch_version: "chromium.3"
  }
  build {
    install: "install.py"
    tool: "chromium/third_party/maven"
    # TODO(crbug.com/1412466): jacoco 0.8.3 can't use the latest chromium jdk (17).
    # This may be changed to chromium normal jdk when jacoco is upgraded to newer versions.
    external_dep: "chromium/third_party/jdk@2@11.0.15+10.cr0"
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}