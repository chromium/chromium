# This suite runs the tests in http/tests/devtools/isolated-code-cache with
# --enable-features=SplitCacheByNetworkIsolationKey
# --disable-site-isolation-trials
# This feature partitions the HTTP cache by network isolation key for improved
# security. This test also disables site isolation, which would cause similar
# results. See the virtual/not-site-per-process README.md for more detail.
# Tracking bug for split cache: crbug.com/910708.
