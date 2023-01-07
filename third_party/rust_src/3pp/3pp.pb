create {
    source {
        script {
            name: "fetch.py"
        }
        unpack_archive: true
    }
}

upload {
    pkg_prefix: "chromium/third_party"
    # This is a source package, so it is not platform-specific
    universal: true
}
