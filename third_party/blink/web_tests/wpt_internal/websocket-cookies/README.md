These tests are for testing third party cookie blocking in conjunction with
WebSockets.

These tests were ported over from http/tests/security/cookies/websocket/.
They need to be here because secure WebSockets are required in order to use
SameSite=None cookies, and wptserve has a wss server but the regular web test
runner does not.

Also see external/wpt/websockets/cookies/third-party-cookie-accepted.https.html,
which would be part of this test suite but it doesn't need to use testRunner
(since it doesn't use third-party cookie blocking) so it can go in external/wpt.
