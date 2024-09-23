= Sanitizer API Virtual Test Suite =

We're removing and then re-implementing the Sanitizer API. In order to not have
to deal with failing sanitizer WPT tests in the mean time, this virtual test
suite is "stealing" all the WPT sanitizer tests.

Note that the virtual test suite definition uses `exclusive_tests: ALL`,
which causes these tests to only be executed as part of this test suite.
