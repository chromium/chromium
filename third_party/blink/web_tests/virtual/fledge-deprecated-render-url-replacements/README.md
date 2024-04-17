This directory is to test deprecatedRenderURLReplacements within Protected Audience with FLEDGE (https://github.com/WICG/turtledove/blob/main/FLEDGE.md) on.

If the `CookieDeprecationFacilitatedTesting` flag is on, we turn off `deprecatedRenderURLReplacements`.

This virtual test is needed to disable `CookieDeprecationFacilitatedTesting` but allow for FLEDGE to be enabled. This way we will properly be able to test `deprecatedRenderURLReplacements`.