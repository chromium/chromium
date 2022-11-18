The prototype Private State Tokens (https://github.com/wicg/trust-token-api)
implementation is going behind an embedder-side flag
(--enable-features=PrivateStateTokens) necessary in addition to the Blink
RuntimeEnabledFeature. This virtual suite allows running the pertinent tests
with the flag on. It can be removed once the flag is enabled by default.
