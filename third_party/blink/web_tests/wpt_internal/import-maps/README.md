# Import maps internal tests

These tests are essentially unit tests of the import map parser. The only
observable effect of an import map is how it impacts module resolution, which is
tested in `external/wpt`. But it's nice to have these sorts of parsing unit
tests too.

## Format

The format is similar to the resolution tests format, which is described in
`external/wpt/import-maps/README.md`. The only differences are that for a given
test object:

* Instead of `expectedResults`, we have `expectedParsedImportMap`.
* `baseURL` is not applicable and cannot appear.
