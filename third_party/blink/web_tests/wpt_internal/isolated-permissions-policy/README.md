This suite is a version of `webexposed/feature-policy-features.html` that
is meant to run under `wptserve`. By running under `wptserve`, a virtual
suite version of this test is also added and that suite runs with a flag
that ensures the `wptserve` origin is isolated, using
`--isolated-context-origins`.
