const otMeta = document.createElement('meta');
otMeta.httpEquiv = 'origin-trial';
// The token below was generated via the following command:
// tools/origin_trials/generate_token.py https://not-web-platform.test:8444 DisableThirdPartyStoragePartitioning2 --expire-timestamp=2000000000 --is-third-party
otMeta.content = 'A+KF3S0fp4lFMijxY8BGc71zYdIGcSd61xM3w9WdjeWduDyCP4Ag0HMwc8H0NqA2WBwBRnJ7IILs/rMvCz4WWw4AAACQeyJvcmlnaW4iOiAiaHR0cHM6Ly9ub3Qtd2ViLXBsYXRmb3JtLnRlc3Q6ODQ0NCIsICJmZWF0dXJlIjogIkRpc2FibGVUaGlyZFBhcnR5U3RvcmFnZVBhcnRpdGlvbmluZzIiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMCwgImlzVGhpcmRQYXJ0eSI6IHRydWV9';
document.head.append(otMeta);
