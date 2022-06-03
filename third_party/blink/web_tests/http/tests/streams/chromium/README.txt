These tests exercise Chromium/Blink specific behavior (like garbage collection,
not crashing, etc) and will not be upstreamed to the W3C web-platform-tests.

The simple-queue-*.html tests originally tested the SimpleQueue class which was
used internally to Blink's Stream API JavaScript implementation. The
implementation has since been replaced, but the tests are retained to prevent
regressions.
