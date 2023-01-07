// Generate this token with the given commands:
// tools/origin_trials/generate_token.py http://localhost:8000 FedCM --is-third-party --expire-timestamp=2000000000
// This token is for "localhost", which is a different origin from the "127.0.0.1" origin used to run all http tests.
const third_party_token = "A2F58fk9agFwXsR5jxSE9v1dVJFQUPdSWWKOs8RiEp1k6jWWMYvPwJqOzg6/GjwydPFLRipD/NeXwTgt7nfPVA4AAABjeyJvcmlnaW4iOiAiaHR0cDovL2xvY2FsaG9zdDo4MDAwIiwgImZlYXR1cmUiOiAiRmVkQ00iLCAiZXhwaXJ5IjogMjAwMDAwMDAwMCwgImlzVGhpcmRQYXJ0eSI6IHRydWV9";

const tokenElement = document.createElement('meta');
tokenElement.httpEquiv = 'origin-trial';
tokenElement.content = third_party_token;
document.head.appendChild(tokenElement);
