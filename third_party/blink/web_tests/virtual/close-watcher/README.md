This is a virtual test suite for the new CloseWatcher feature. It was simply
enabled in experimental web platform features, but since it causes some
interop2022 tracked WPTs to fail, we need to keep it out of experimental web
platform features for now. Since it isn't in experimental web platform features,
we need this virtual test suite in order to get any test coverage.

Flag: --enable-blink-features=CloseWatcher
Bug: crbug.com/1171318
