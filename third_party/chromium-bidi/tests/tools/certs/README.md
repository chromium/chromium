# Test certificates

This folder contains self-issued certificates used for e2e testing. There are
2 pairs of certificates: `ssl_bad...` and `ssl_good...`. They are identical except
the fact that `ssl_good`'s SPKI is passed in Chrome's
`--ignore-certificate-errors-spki-list` flag, so it is considered a valid in e2e
tests.

# Update certificates

In order to re-generate certificates, you need make the following steps using
`openssl`:

## 1. Generate private keys

```bash
openssl genpkey -algorithm RSA -out ssl_good.key
openssl genpkey -algorithm RSA -out ssl_bad.key
```

## 2. Generate certificates

```bash
openssl req -new -x509 -key ssl_good.key -out ssl_good.crt -days 9999 -subj "/C=US"
openssl req -new -x509 -key ssl_bad.key -out ssl_bad.crt -days 9999 -subj "/C=US"
 ```

## 3. Update `conftest.py`

In order to make Chrome under test consider the `ssl_good` certificate as a valid
one, the `--ignore-certificate-errors-spki-list` cli argument is used with SPKI of
the `ssl_good` certificate.

### 3.1. Get `ssl_good`'s SPKI

```bash
openssl x509 -in ssl_good.crt -pubkey -noout | openssl pkey -pubin -outform der | openssl dgst -sha256 -binary | base64
```

### 3.2. Update `tests/conftest.py`

Update the [`tests/conftest.py:GOOD_SSL_CERT_SPKI`](https://github.com/GoogleChromeLabs/chromium-bidi/blob/b41d47588bb826082a69688174e44f904b134122/tests/conftest.py#L37) constant with the new SPKI
from the previous step.

## 4. Verify the new certificate works

Run this e2e tests to verify the configuration works as expected:
```bash
npm run e2e -- -k test_browsing_context_navigate_ssl
```
