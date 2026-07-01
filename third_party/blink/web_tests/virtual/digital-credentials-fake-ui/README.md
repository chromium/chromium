# digital-credentials-fake-ui

This virtual test suite runs Digital Credentials tests with the fake UI
configured to auto-resolve.

By default, in `content_shell`, Digital Credentials requests hang (simulating
the browser waiting for the user to interact with the UI).
This virtual suite enables the fake UI by passing
`--use-fake-ui-for-digital-identity`, which causes the requests to
auto-resolve with a fake token in 1ms, allowing testing of successful token
retrieval.
