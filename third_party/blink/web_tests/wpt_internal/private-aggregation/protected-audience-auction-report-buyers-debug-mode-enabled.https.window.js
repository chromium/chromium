// META: timeout=long
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=../aggregation-service/support/aggregation-service.js
// META: script=resources/utils.js
// META: script=/private-aggregation/resources/protected-audience-helper-module.js

'use strict';

const reportPoller = new ReportPoller(
    '/.well-known/private-aggregation/report-protected-audience',
    '/.well-known/private-aggregation/debug/report-protected-audience',
    /*fullTimeoutMs=*/ 5000,
);


private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const joinAdInterestGroup = {
    sellerCapabilities: {'*': ['interest-group-counts']}
  };
  const runAdAuction = {
    auctionReportBuyerKeys: [1n],
    auctionReportBuyers: {interestGroupCount: {bucket: 0n, scale: 2}},
    auctionReportBuyerDebugModeConfig: {enabled: true}
  };

  await runReportTest(
      test, uuid, {}, /*expectedNumReports=*/ 0,
      /*overrides=*/ {joinAdInterestGroup, runAdAuction});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'auctionReportBuyerDebugModeConfig with enabled true');


private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const joinAdInterestGroup = {
    sellerCapabilities: {'*': ['interest-group-counts']}
  };
  const runAdAuction = {
    auctionReportBuyerKeys: [1n],
    auctionReportBuyers: {interestGroupCount: {bucket: 0n, scale: 2}},
    auctionReportBuyerDebugModeConfig: {enabled: true, debugKey: 1234n}
  };

  await runReportTest(
      test, uuid, {}, /*expectedNumReports=*/ 0,
      /*overrides=*/ {joinAdInterestGroup, runAdAuction});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ '1234',
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'auctionReportBuyerDebugModeConfig with enabled true and debug key');


private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const joinAdInterestGroup = {
    sellerCapabilities: {'*': ['interest-group-counts']}
  };
  const runAdAuction = {
    auctionReportBuyerKeys: [1n],
    auctionReportBuyers: {interestGroupCount: {bucket: 0n, scale: 2}},
    auctionReportBuyerDebugModeConfig: {enabled: true, debugKey: -1n}
  };

  promise_rejects_js(
      test, TypeError,
      runReportTest(
          test, uuid, {}, /*expectedNumReports=*/ 0,
          /*overrides=*/ {joinAdInterestGroup, runAdAuction}));

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'auctionReportBuyerDebugModeConfig with negative debug key');


private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const joinAdInterestGroup = {
    sellerCapabilities: {'*': ['interest-group-counts']}
  };
  const runAdAuction = {
    auctionReportBuyerKeys: [1n],
    auctionReportBuyers: {interestGroupCount: {bucket: 0n, scale: 2}},
    auctionReportBuyerDebugModeConfig: {enabled: true, debugKey: 1n << 64n}
  };

  promise_rejects_js(
      test, TypeError,
      runReportTest(
          test, uuid, {}, /*expectedNumReports=*/ 0,
          /*overrides=*/ {joinAdInterestGroup, runAdAuction}));

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'auctionReportBuyerDebugModeConfig with too large debug key');
