# This suite runs the tests in http/tests/wasm/caching with
# --js-flags=--allow-natives-syntax.
# This feature is required to detect when caching has happened. The flag could
# be avoided by making sure the test runs long enough. However, this would
# either make the test slow on fast bots, or flaky on slow bots.
