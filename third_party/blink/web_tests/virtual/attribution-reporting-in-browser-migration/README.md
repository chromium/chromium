# Tests for Attribution Reporting APIs with the in-browser migration enabled

This virtual test suite covers a subset of Attribution Reporting APIs tests with
the `KeepAliveInBrowserMigration` and `AttributionReportingInBrowserMigration`
flags enabled.

These flags enable a new flow where attribution responses are processed in the
browser process instead of blink.

The suite only covers tests related to devtools reporting. Other attribution
reporting API tests are not enabled as running them would interfere with the
existing suite.
