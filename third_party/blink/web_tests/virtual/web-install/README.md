This virtual test suite runs web-install WPT tests with the
WebAppInstallation feature enabled via --enable-features=WebAppInstallation.

These tests cover navigator.install() precondition checks (sandbox, iframe,
user activation restrictions) and input validation that can be tested without
actually installing a web app.
