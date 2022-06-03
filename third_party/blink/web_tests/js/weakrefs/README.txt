JS WeakRefs Tests README

These tests are currently Chrome and V8-specific. They pass because of the
exposed gc() function, which forces a major GC synchronously and forces
collection of the unreachable `garbage` binding in the tests.

Moving these tests to WPT is blocked on [a cross-browser way to trigger GC in
the test harness](https://github.com/web-platform-tests/wpt/issues/7899).
