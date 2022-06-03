This directory contains tests that require disabling the automatic process
isolation of web platform test origins. That isolation is usually useful for
increased test coverage, but in some tests it interferes with the system under
test. See https://crbug.com/1073181 for background.

This is separate from the `not-site-per-process` virtual test suite, which
disables site isolation entirely. Tests running under this virtual test suite
benefit from _site_ isolation (e.g. isolating `not-web-platform.test` from
`web-platform.test`). They just do not get automatic origin isolation (e.g.
isolating `web-platform.test` from `www.web-platform.test`).
