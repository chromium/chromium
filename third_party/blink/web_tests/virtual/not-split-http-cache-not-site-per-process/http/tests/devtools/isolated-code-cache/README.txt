# This suite runs the tests in http/tests/devtools/isolated-code-cache with
# --disable-features=SplitCacheByNetworkIsolationKey
# --disable-site-isolation-trials
# This disables the feature which partitions the HTTP cache by network isolation
# key for improved security as well as site isolation. See the
# virtual/not-site-per-process README.md for more detail.
# Tracking bug for split cache: crbug.com/910708.
