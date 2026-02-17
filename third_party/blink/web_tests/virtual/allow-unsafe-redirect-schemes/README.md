# Tests for allowing unsafe redirect schemes in manual redirect mode

This virtual test suite runs fetch redirect tests with the
`AllowUnsafeRedirectSchemesForManualMode` feature enabled.

When enabled, fetch() requests with `redirect: "manual"` can receive
opaque-redirect responses for redirects to non-HTTP(S) schemes (like `data:`),
per the Fetch spec. The redirect URL is censored to `data:,` for security.

See https://fetch.spec.whatwg.org/#http-redirect-fetch step 9.
