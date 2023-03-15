create {
  source {
    cipd {
      pkg: 'chromium/third_party/espresso'
      default_version: '2@2.2.1'
      # Doesnt really exist anymore but thats where the Readme says it was from.
      original_download_url: 'https://google.github.io/android-testing-support-library/docs/espresso/'
    }
    patch_version: 'cr1'
  }

  build {
    dep: 'chromium/third_party/android_build_tools/jetifier'
    dep: 'chromium/third_party/jdk'
  }
}

upload {
  universal: true
  pkg_prefix: 'chromium/third_party'
}
