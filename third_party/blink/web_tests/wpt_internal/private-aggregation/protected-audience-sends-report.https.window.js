// META: timeout=long
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=../aggregation-service/support/aggregation-service.js
// META: script=resources/utils.js
// META: script=/private-aggregation/resources/protected-audience-helper-module.js

'use strict';

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
  });

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
}, 'auction that calls Private Aggregation in generateBid');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    scoreAd: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
  });

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
}, 'auction that calls Private Aggregation in scoreAd');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
  });

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
}, 'auction that calls Private Aggregation in reportWin');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    reportResult: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
  });

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
}, 'auction that calls Private Aggregation in reportResult');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    reportResult: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          MULTIPLE_CONTRIBUTIONS_EXAMPLE,
          NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience');
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
}, 'auction that calls Private Aggregation with multiple contributions');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`,
    reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          MULTIPLE_CONTRIBUTIONS_EXAMPLE,
          NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience');
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
}, 'auction that calls Private Aggregation batches across different functions');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    reportResult:
        `privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
  });

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
}, 'auction that calls Private Aggregation without debug mode');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    reportResult: `privateAggregation.enableDebugMode({ debugKey: 1234n });
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
  });

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
}, 'auction that calls Private Aggregation using a debug key');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode({ debugKey: 1234n });
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`,
    reportWin: `privateAggregation.enableDebugMode({ debugKey: 2345n });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience',
      /*wait_for=*/ 2);
  assert_equals(reports.length, 2);

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience',
      /*wait_for=*/ 2);
  assert_equals(debug_reports.length, 2);
}, 'auction that calls Private Aggregation does not batch different functions if debug keys differ');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid:
        `privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`,
    reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience',
      /*wait_for=*/ 2);
  assert_equals(reports.length, 2);

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience');
  assert_equals(debug_reports.length, 1);
}, 'auction that calls Private Aggregation does not batch different functions if debug modes differ');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`,
    reportWin: `privateAggregation.enableDebugMode({ debugKey: 1234n });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience',
      /*wait_for=*/ 2);
  assert_equals(reports.length, 2);

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience',
      /*wait_for=*/ 2);
  assert_equals(debug_reports.length, 2);
}, 'auction that calls Private Aggregation does not batch different functions if debug key presence differs');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
  });

  await runReportTest(test, uuid, {
    reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience',
      /*wait_for=*/ 2);
  assert_equals(reports.length, 2);

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience',
      /*wait_for=*/ 2);
  assert_equals(debug_reports.length, 2);
}, 'auctions that call Private Aggregation do not batch across different auctions');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });
        aNonExistentVariable;`
  });

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
}, 'auction that calls Private Aggregation then has unrelated error still sends reports');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 3 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 1 });`
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          MULTIPLE_CONTRIBUTIONS_EXAMPLE,
          NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience');
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
}, 'auction that calls Private Aggregation with mergeable contributions');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 0 });`
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          MULTIPLE_CONTRIBUTIONS_EXAMPLE,
          NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience');
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
}, 'auction that calls Private Aggregation with zero-value contributions');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        for (let i = 1n; i <= ${
        NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE +
        1}n; i++) {  // Too many contributions
          privateAggregation.contributeToHistogram({ bucket: i, value: 1 });
        }`
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildPayloadWithSequentialContributions(
          NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience');
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
}, 'auction that calls Private Aggregation with too many contributions');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        // Too many contributions ignoring merging
        for (let i = 1n; i <= 21n; i++) {
          privateAggregation.contributeToHistogram({ bucket: 1n, value: 1 });
        }`
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_HIGHER_VALUE_EXAMPLE,
          NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience');
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
}, 'auction that calls Private Aggregation with many mergeable contributions');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        // Sums to value 1 if overflow is allowed.
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2147483647 });
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2147483647 });
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 3 });`
  });

  // No reports are expected as the budget has surely been exceeded.
  const reports = await pollReports(
      '/.well-known/private-aggregation/report-shared-storage',
      /*wait_for=*/ 1, /*timeout=*/ 50)
  assert_equals(reports, null);

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-shared-storage',
      /*wait_for=*/ 1, /*timeout=*/ 50)
  assert_equals(debug_reports, null);
}, 'auction that calls Private Aggregation with values that sum to more than the max long');
