This directory contains tests for the CSSCustomStateDeprecatedSyntax flag
enabled and the CSSCustomStateNewSyntax flag disabled. We are shipping
:state(foo) as a replacement for :--foo, and we will initially be shipping the
deprecated syntax enabled and the new syntax disabled on stable, which this
virtual test suite tests.

https://bugs.chromium.org/p/chromium/issues/detail?id=1514397
