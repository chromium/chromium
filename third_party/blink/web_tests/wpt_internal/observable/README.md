# Observable API internal tests

This is the `wpt_internal` directory for DOM Observable tests. Tests in here
cannot be upstreamed to https://github.com/web-platform-tests/wpt/ because they
rely on various Chrome internals like the `internals` API, or v8-specific
test-only internals like the `gc()` global.
