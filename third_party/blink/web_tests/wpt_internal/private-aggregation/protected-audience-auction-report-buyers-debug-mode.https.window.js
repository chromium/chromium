// META: timeout=long
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=../aggregation-service/support/aggregation-service.js
// META: script=resources/utils.js
// META: script=/private-aggregation/resources/protected-audience-helper-module.js

'use strict';

private_aggregation_promise_test(async test => {
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

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ false, /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined);

  // We use a short timeout as the previous poll should've waited long enough.
  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience',
      /*wait_for=*/ 1, /*timeout=*/ 50);
  assert_equals(debug_reports, null);
}, 'auctionReportBuyerDebugModeConfig missing');

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

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience');
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
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

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ '1234',
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience');
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
}, 'auctionReportBuyerDebugModeConfig with enabled true and debug key');

private_aggregation_promise_test(async test => {
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

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ false, /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined);

  // We use a short timeout as the previous poll should've waited long enough.
  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience',
      /*wait_for=*/ 1, /*timeout=*/ 50);
  assert_equals(debug_reports, null);
}, 'auctionReportBuyerDebugModeConfig with enabled false');

private_aggregation_promise_test(async test => {
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

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ false, /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined);

  // We use a short timeout as the previous poll should've waited long enough.
  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience',
      /*wait_for=*/ 1, /*timeout=*/ 50);
  assert_equals(debug_reports, null);
}, 'auctionReportBuyerDebugModeConfig empty');

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

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports, null);

  // We use a short timeout as the previous poll should've waited long enough.
  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience',
      /*wait_for=*/ 1, /*timeout=*/ 50);
  assert_equals(debug_reports, null);
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

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports, null);

  // We use a short timeout as the previous poll should've waited long enough.
  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience',
      /*wait_for=*/ 1, /*timeout=*/ 50);
  assert_equals(debug_reports, null);
}, 'auctionReportBuyerDebugModeConfig with too large debug key');

private_aggregation_promise_test(async test => {
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

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports, null);

  // We use a short timeout as the previous poll should've waited long enough.
  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience',
      /*wait_for=*/ 1, /*timeout=*/ 50);
  assert_equals(debug_reports, null);
}, 'auctionReportBuyerDebugModeConfig with debug key and enabled false');

private_aggregation_promise_test(async test => {
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

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports, null);

  // We use a short timeout as the previous poll should've waited long enough.
  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience',
      /*wait_for=*/ 1, /*timeout=*/ 50);
  assert_equals(debug_reports, null);
}, 'auctionReportBuyerDebugModeConfig not a dictionary');
