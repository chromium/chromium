const meta = document.createElement('meta');
meta.httpEquiv = "origin-trial";
// This third party Origin Trial token is generated with the command:
//   tools/origin_trials/generate_token.py \
//     --expire-timestamp=2000000000 \
//     --version=3 \
//     --is-third-party \
//     https://localhost:8443 SubresourceWebBundles
meta.content = "A9xwATdiRbEilMSvfDeUW1WDxl/YfIJoQgg0vFmKmnFJ0TNxJh0WCro4TaA2MOhlyrz/lHT+wXBDLiRF9i9c1AEAAAB0eyJvcmlnaW4iOiAiaHR0cHM6Ly9sb2NhbGhvc3Q6ODQ0MyIsICJpc1RoaXJkUGFydHkiOiB0cnVlLCAiZmVhdHVyZSI6ICJTdWJyZXNvdXJjZVdlYkJ1bmRsZXMiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=";
document.head.appendChild(meta);
