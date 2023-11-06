This directory is to test rounding values feature for Protected Audience with FLEDGE (https://github.com/WICG/turtledove/blob/main/FLEDGE.md) on.

The rounding value feature is governed by the `FledgeRounding` flag and consists of three distinct detailed configurations:
* `fledge_bid_reporting_bits`: `browserSignals.bid` of `generateBid()`
* `fledge_score_reporting_bits`: `browserSignals.desirability`, `browserSignals.highestScoringOtherBid` of `scoreAd()`
* `fledge_ad_cost_reporting_bits`: `browserSignals.adCost` of `generateBid()`
