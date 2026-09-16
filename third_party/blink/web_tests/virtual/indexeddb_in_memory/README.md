# IndexedDB in-memory tests

This runs Blob- and FSA-related IndexedDB tests under an in-memory context
because the backend code for it differs from persistent contexts.

Note that the --incognito flag works for `headless_shell` but not
`content_shell`, so `wpt_internal` tests can't be included here yet. See
`//docs/testing/run_web_platform_tests.md`.
