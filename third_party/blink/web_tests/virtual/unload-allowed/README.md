Tests that fail when unload is deprecated (--enable-features=DeprecateUnload).
See https://crbug.com/1488371. DeprecateUnload is on by default for tests, the
expectations here are for tests that require unload (usually because they are
explicitly testing it).
