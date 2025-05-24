// META: variant=?include=badConfig
// META: variant=?include=enabledFalse
// META: timeout=long
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/common/subset-tests-by-key.js
// META: script=../aggregation-service/support/aggregation-service.js
// META: script=resources/utils.js
// META: script=/private-aggregation/resources/protected-audience-helper-module.js

'use strict';

const reportPoller = new ReportPoller(
    '/.well-known/private-aggregation/report-protected-audience',
    '/.well-known/private-aggregation/debug/report-protected-audience',
    /*fullTimeoutMs=*/ 5000,
);

subsetTestByKey('badConfig', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();

  const joinAdInterestGroup = {
    sellerCapabilities: {'*': ['interest-group-counts']}
  };
  const runAdAuction = {
    auctionReportBuyerKeys: [1n],
    auctionReportBuyers: {interestGroupCount: {bucket: 0n, scale: 2}}
  };

  await runReportTest(
      test, uuid, {}, /*expectedNumReports=*/ 0,
      /*overrides=*/ {joinAdInterestGroup, runAdAuction});

  const {reports: [report]} = await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ false, /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined);
}, 'auctionReportBuyerDebugModeConfig missing');

subsetTestByKey(
    'enabledFalse', private_aggregation_promise_test, async test => {
      const uuid = generateUuid();

      const joinAdInterestGroup = {
        sellerCapabilities: {'*': ['interest-group-counts']}
      };
      const runAdAuction = {
        auctionReportBuyerKeys: [1n],
        auctionReportBuyers: {interestGroupCount: {bucket: 0n, scale: 2}},
        auctionReportBuyerDebugModeConfig: {enabled: false}
      };

      await runReportTest(
          test, uuid, {}, /*expectedNumReports=*/ 0,
          /*overrides=*/ {joinAdInterestGroup, runAdAuction});

      const {reports: [report]} = await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);

      verifyReport(
          report, /*api=*/ 'protected-audience',
          /*is_debug_enabled=*/ false, /*debug_key=*/ undefined,
          /*expected_payload=*/ undefined);
    }, 'auctionReportBuyerDebugModeConfig with enabled false');

subsetTestByKey('badConfig', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();

  const joinAdInterestGroup = {
    sellerCapabilities: {'*': ['interest-group-counts']}
  };
  const runAdAuction = {
    auctionReportBuyerKeys: [1n],
    auctionReportBuyers: {interestGroupCount: {bucket: 0n, scale: 2}},
    auctionReportBuyerDebugModeConfig: {}
  };

  await runReportTest(
      test, uuid, {}, /*expectedNumReports=*/ 0,
      /*overrides=*/ {joinAdInterestGroup, runAdAuction});

  const {reports: [report]} = await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ false, /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined);
}, 'auctionReportBuyerDebugModeConfig empty');

subsetTestByKey(
    'enabledFalse', private_aggregation_promise_test, async test => {
      const uuid = generateUuid();

      const joinAdInterestGroup = {
        sellerCapabilities: {'*': ['interest-group-counts']}
      };
      const runAdAuction = {
        auctionReportBuyerKeys: [1n],
        auctionReportBuyers: {interestGroupCount: {bucket: 0n, scale: 2}},
        auctionReportBuyerDebugModeConfig: {enabled: false, debugKey: 1234n}
      };

      promise_rejects_js(
          test, TypeError,
          runReportTest(
              test, uuid, {}, /*expectedNumReports=*/ 0,
              /*overrides=*/ {joinAdInterestGroup, runAdAuction}));

      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
    }, 'auctionReportBuyerDebugModeConfig with debug key and enabled false');


subsetTestByKey('badConfig', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();

  const joinAdInterestGroup = {
    sellerCapabilities: {'*': ['interest-group-counts']}
  };
  const runAdAuction = {
    auctionReportBuyerKeys: [1n],
    auctionReportBuyers: {interestGroupCount: {bucket: 0n, scale: 2}},
    auctionReportBuyerDebugModeConfig: 123
  };

  promise_rejects_js(
      test, TypeError,
      runReportTest(
          test, uuid, {}, /*expectedNumReports=*/ 0,
          /*overrides=*/ {joinAdInterestGroup, runAdAuction}));

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'auctionReportBuyerDebugModeConfig not a dictionary');
