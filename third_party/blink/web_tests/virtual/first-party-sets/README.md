The prototype implementation of [First-Party
Sets](https://github.com/privacycg/first-party-sets) is behind an
embedder-side flag (`--enable-features=FirstPartySets`, or
`--use-first-party-set="..."`). This virtual suite allows tests to run with
the flag enabled, and a First-Party Set specified, in order to validate the
relevant functionality. It can be removed or made non-virtual once the flag
is enabled by default.