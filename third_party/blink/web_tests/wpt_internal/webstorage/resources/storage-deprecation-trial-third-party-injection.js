const otMeta = document.createElement('meta');
otMeta.httpEquiv = 'origin-trial';
// The token below was generated via the following command:
// tools/origin_trials/generate_token.py https://not-web-platform.test:8444 DisableThirdPartyStoragePartitioning --expire-timestamp=2000000000 --is-third-party
otMeta.content = 'A5e8lolpivDBfLwsRrIt4gO7FEOMwRvhqcyjs4GXE2KHSQr1xsWEQwYdmf/ZvFngnLDAtmkvLZ+/LrB1C2xfdwEAAACPeyJvcmlnaW4iOiAiaHR0cHM6Ly9ub3Qtd2ViLXBsYXRmb3JtLnRlc3Q6ODQ0NCIsICJmZWF0dXJlIjogIkRpc2FibGVUaGlyZFBhcnR5U3RvcmFnZVBhcnRpdGlvbmluZyIsICJleHBpcnkiOiAyMDAwMDAwMDAwLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZX0=';
document.head.append(otMeta);
