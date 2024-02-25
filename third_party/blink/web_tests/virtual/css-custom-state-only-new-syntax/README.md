This directory contains tests for the CSSCustomStateDeprecatedSyntax flag
disabled and the CSSCustomStateNewSyntax flag enabled. We are shipping
:state(foo) as a replacement for :--foo, and this virtual test suite tests the
configuration which we are eventually going to ship to stable after the
deprecation is done.

https://bugs.chromium.org/p/chromium/issues/detail?id=1514397
