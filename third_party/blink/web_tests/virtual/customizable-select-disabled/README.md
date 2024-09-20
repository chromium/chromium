This is a virtual test suite which makes sure that the new behavior of the
CustomizableSelect feature, especially the parser changes, doesn't leak into stable
chrome while we are still working on the experimental version. It also makes
sure that UseCounters for parser compatibility are running on stable.

This virtual test suite can be deleted when the CustomizableSelect flag is
enabled by default or removed.

Flag: --disable-features=CustomizableSelect
Bug: crbug.com/1511354
