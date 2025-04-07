# virtual/bypass-csp-for-preloads-disabled

This directory is for tests that need the BypassCSPForPreloads feature flag
disabled to match the experimental behavior where CSP is not bypassed for
preloads.
Tests under `virtual/bypass-csp-for-preloads-disabled` are run with
`--disable-features=BypassCSPForPreloads` and
`--enable-features=PreloadLinkRelDataUrls`.
