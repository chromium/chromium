const otMeta = document.createElement('meta');
otMeta.httpEquiv = 'origin-trial';
// The token below was generated via the following command:
// tools/origin_trials/generate_token.py https://not-web-platform.test:8444 DisableThirdPartyStoragePartitioning3 --expire-timestamp=2000000000 --is-third-party
otMeta.content = 'A1hitvOMiEfkhTsRmwePmMhES5XD+LjGCvTZOE64etV2v0Jr+z8nQXJWF2GYsK0RMaRA/Uz6PkHX/HmNq6aElQAAAACQeyJvcmlnaW4iOiAiaHR0cHM6Ly9ub3Qtd2ViLXBsYXRmb3JtLnRlc3Q6ODQ0NCIsICJmZWF0dXJlIjogIkRpc2FibGVUaGlyZFBhcnR5U3RvcmFnZVBhcnRpdGlvbmluZzMiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMCwgImlzVGhpcmRQYXJ0eSI6IHRydWV9';
document.head.append(otMeta);
