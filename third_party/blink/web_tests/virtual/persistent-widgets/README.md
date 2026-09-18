This virtual test suite is needed because setting status to "test" or
"experimental" in runtime_enabled_features.json5 does not enable the feature in
the browser process during tests like we want it to. In order to enable it in
the browser process, we have to pass the the --enable-features argument to the
browser.

Flags: `--enable-features=PersistentWidgetAPI`
