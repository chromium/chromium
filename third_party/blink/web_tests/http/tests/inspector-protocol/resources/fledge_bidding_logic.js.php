<?php
header("Content-Type: application/javascript");
header("Ad-Auction-Allowed: true");
?>
// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  console.log("generateBid running");
  return {'ad': 'example', 'bid': 1 + Number(interestGroup.name),
          'render': interestGroup.ads[0].renderURL};
}

function reportWin(
    auctionSignals, perBuyerSignals, sellerSignals,
    browserSignals) {
  sendReportTo(browserSignals.interestGroupOwner + '/echoall?report_bidder');
}
