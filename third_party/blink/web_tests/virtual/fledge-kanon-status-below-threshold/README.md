This directory is to test kAnonStatus in reportWin() for Protected Audience with FLEDGE (https://github.com/WICG/turtledove/blob/main/FLEDGE.md) on.

This parameter has four enum string values:
* `passedAndEnforced` The ad was k-anonymous and k-anonymity was required to win the auction.
* `passedNotEnforced` The ad was k-anonymous though k-anonymity was not required to win the auction.
* `belowThreshold` The ad was not k-anonymous but k-anonymity was not required to win the auction.
* `notCalculated` The browser did not calculate the k-anonymity status of the ad, and k-anonymity was not required to win the auction.

The `kAnonStatus` is governed by the following flags: `CookieDeprecationFacilitatedTesting`, `FledgeEnforceKAnonymity`, and `FledgeConsiderKAnonymity`.

If `CookieDeprecationFacilitatedTesting` is enabled, `KAnonymityBidMode` is set to `kNone`, resulting in a `kAnonStatus` of `notCalculated`.

If `CookieDeprecationFacilitatedTesting` is disabled, and `FledgeConsiderKAnonymity` is enabled while `FledgeEnforceKAnonymity` is disabled, it results in a `kAnonStatus` of `belowThreshold`.

The values `passedAndEnforced` and `passedNotEnforced` are determined by the Chrome KAnonymous service, but due to the complexity of simulating this service in web platform tests, they are currently omitted from the test.
