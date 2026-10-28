This suite runs tests with `--disable-features=WebAppInstallation` to verify
that navigator.install() is not exposed when the feature is disabled. It can
be removed when the WebAppInstallation kill switch is removed.
