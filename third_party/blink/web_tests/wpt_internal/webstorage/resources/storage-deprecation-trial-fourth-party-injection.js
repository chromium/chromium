// We inject this to verify that multiple tokens can be applied at once,
// that the first token to be set doesn't 'win'.
const otMetaUnused = document.createElement('meta');
otMetaUnused.httpEquiv = 'origin-trial';
// The token below was generated via the following command:
// tools/origin_trials/generate_token.py https://www.web-platform.test:8444 DisableThirdPartyStoragePartitioning2 --expire-timestamp=2000000000 --is-third-party
otMetaUnused.content = 'A8M/hcRNIjs7/IMza0eiCurOxKPRicrY0Maz8Py4fvj6GOC1yrbiw+1+p1wxbzDoV4IiZuNA/SaBaICkMfpQ+g4AAACQeyJvcmlnaW4iOiAiaHR0cHM6Ly93d3cud2ViLXBsYXRmb3JtLnRlc3Q6ODQ0NCIsICJmZWF0dXJlIjogIkRpc2FibGVUaGlyZFBhcnR5U3RvcmFnZVBhcnRpdGlvbmluZzIiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMCwgImlzVGhpcmRQYXJ0eSI6IHRydWV9';
document.head.append(otMetaUnused);
