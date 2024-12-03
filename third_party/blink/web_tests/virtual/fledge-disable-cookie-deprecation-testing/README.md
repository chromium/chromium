This directory is to test some Protected Audience (https://github.com/WICG/turtledove/blob/main/FLEDGE.md) features with `CookieDeprecationFacilitatedTesting` turned off.

If the `CookieDeprecationFacilitatedTesting` flag is on, we turn off:
* `FledgeDeprecatedRenderURLReplacements`.
* `FledgeRealTimeReporting`

This virtual test is needed to disable `CookieDeprecationFacilitatedTesting` to allow for:
* `FledgeDeprecatedRenderURLReplacements` to be enabled to test deprecatedRenderURLReplacements.
* `FledgeRealTimeReporting` to be enabled to test real time reporting API.