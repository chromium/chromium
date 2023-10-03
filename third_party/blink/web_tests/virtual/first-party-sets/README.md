The prototype implementation of [Related Website
Sets](https://github.com/privacycg/first-party-sets) is behind an
embedder-side flag (`--enable-features=FirstPartySets`, or
`--use-related-website-set="..."`). This virtual suite allows tests to run with
the flag enabled, and a Related Webste Set specified, in order to validate the
relevant functionality. It can be removed or made non-virtual once the flag
is enabled by default.