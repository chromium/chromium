This suite runs the tests with the
`--enable-fake-no-alloc-direct-call-for-testing` flag, forcing the V8/Blink
bindings to call all API entry points that use the `[NoAllocDirectCall]`
extended IDL attribute as if V8 were using the fast call code path. The goal is
to guarantee test coverage for the Blink side of V8 Fast API calls,
independently of whether or not V8 actually activates the fast path, which
depends on heuristics.

Test crashes that are specific to this suite are most likely caused by forbidden
V8 allocations or JavaScript execution within the scopes of calls to blink APIs
that have the `[NoAllocDirectCall]` extended IDL attribute.

Point of contact: junov@chromium.org
