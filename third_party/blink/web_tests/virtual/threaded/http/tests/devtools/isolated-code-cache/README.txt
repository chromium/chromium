This suite runs the tests in devtools/isolated-code-cache with a threaded
compositor. This is required for requestIdleCallback to work, which is required
for --enable-features=CacheCodeOnIdle.
