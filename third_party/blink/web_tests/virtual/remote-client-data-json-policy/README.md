# remoteClientDataJSON Permissions Policy virtual test suite

This suite runs the remoteClientDataJSON Permissions Policy tests with
`--enable-features=WebAuthnRemoteClientDataJson`.

The tests use structurally invalid client data to distinguish an allowed request
(`EncodingError`) from a policy-blocked request (`NotAllowedError`) without
invoking an authenticator.
