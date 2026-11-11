# FedCmDelegation
This suite runs the tests in external/wpt/fedcm/fedcm-delegation/
with `--enable-features=EmailVerificationProtocol`.

The Email Verification Protocol identifies an issuer by a bare host name, so
the tests reach it on the default HTTPS port. Every test in that directory
therefore needs

```html
<meta name="pac" content="/fedcm/support/proxy.sub.pac">
```

which routes that port to the wpt test server.
