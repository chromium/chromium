Import assertions are being removed in favor of import attributes. The
flag is changed to disabled by default, and tests are kept as-is, so
keep running them by explicitly passing the flag. Once we have
confidence it is web compatible to remove, these tests will be updated
in https://github.com/web-platform-tests/wpt/pull/46020 and the virtual
suite will be removed.
