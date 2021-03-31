This suite runs the tests in web_tests/fast/canvas/ with the
--enable-fake-no-alloc-direct-call-for-testing flag. Test crashes that
are specific to this suite are most likely caused by forbidden V8
allocations or JavaScript execution within the scopes of calls to blink
APIs that have the [NoAllocDirectCall] extended IDL attribute.

Point of contact: junov@chromium.org
