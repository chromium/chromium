// These functions are used by FLEDGE to determine the logic for the ad buyer.
// For our testing purposes, we only need the minimal amount of boilerplate
// code in place to allow them to be invoked properly and move the FLEDGE
// process along. The tests do not deal with reporting results, so we leave
// `reportWin` empty. See `generateURNFromFledge` in "utils.js" to see how
// these files are used.

function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1,
          'render': interestGroup.ads[0].renderUrl};
}

function reportWin(
    auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
  return;
}
