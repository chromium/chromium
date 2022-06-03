# Web Share layout tests

The main body of Web Share tests are found in
[`external/wpt/web-share`](../external/wpt/web-share) (imported from Web
Platform Tests). However, many of those tests are necessarily manual
(since they involve user interaction).

The tests in this directory are Chromium-specific automated versions of
the manual tests from WPT. They use user-gesture hacks and a mock Mojo
implementation to fully automate the API tests. These should be kept in
sync with the upstream manual versions of the tests.
