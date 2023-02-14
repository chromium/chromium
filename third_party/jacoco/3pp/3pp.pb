create {
  source {
    url {
      download_url: "https://repo1.maven.org/maven2/org/jacoco/jacoco/0.8.8/jacoco-0.8.8.zip"
      version: "0.8.8"
      extension: ".zip"
    }
    unpack_archive: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  universal: true
}