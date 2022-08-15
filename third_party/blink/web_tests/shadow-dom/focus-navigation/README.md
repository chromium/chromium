# Focus Navigation Tests Pending Upstream to WPT

Tests in this folder are all upstreamed to WPT at
`third_party/blink/web_test/external/wpt/shadow-dom/focus-navigation`.
We are waiting for crbug.com/769673 and crbug.com/893480 to be resolved
before enabling these tests in WPT.

Once fixed, make sure to:
- Delete this folder and its content.
- Enable the tests in WPT by removing failures from TestExpectations.