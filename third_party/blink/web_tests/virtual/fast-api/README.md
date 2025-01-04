# This suite runs the tests in bindings/ with --js-flags=--allow-natives-syntax.
# Fast api tests require that the test function gets optimized with the top tier
# compiler of V8, and there is no way to achieve this consistently and
# deterministically  without V8-internal testing functions.
