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
        privateAggregation.contributeToHistogram(
            {bucket: 1n, value: 2, filteringId: 3n});`
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
          ONE_CONTRIBUTION_WITH_FILTERING_ID_EXAMPLE,
          NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience');
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
}, 'auction that calls Private Aggregation with a non-default filtering ID');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.contributeToHistogram(
            {bucket: 1n, value: 2, filteringId: 3n});`
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
      /*wait_for=*/ 1, /*timeout=*/ 50)
  assert_equals(debug_reports, null);
}, 'auction that calls Private Aggregation with a non-default filtering ID and no debug mode');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({bucket: 1n, value: 2});`
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
}, 'auction that calls Private Aggregation with no filtering ID specified');


private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram(
            {bucket: 1n, value: 2, filteringId: 0n});`
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
}, 'auction that calls Private Aggregation with an explicitly default filtering ID');


private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.contributeToHistogram(
                      {bucket: 1n, value: 2, filteringId: 255n});`
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
      /*wait_for=*/ 1, /*timeout=*/ 50)
  assert_equals(debug_reports, null);
}, 'auction that calls Private Aggregation with max filtering ID');


private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  promise_rejects_js(test, TypeError, runReportTest(test, uuid, {
                       generateBid: `privateAggregation.contributeToHistogram(
                          {bucket: 1n, value: 2, filteringId: 256n});`
                     }));

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports, null);

  // We use a short timeout as the previous poll should've waited long enough.
  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience',
      /*wait_for=*/ 1, /*timeout=*/ 50)
  assert_equals(debug_reports, null);
}, 'auction that calls Private Aggregation with filtering ID too big');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  promise_rejects_js(test, TypeError, runReportTest(test, uuid, {
                       generateBid: `privateAggregation.contributeToHistogram(
                          {bucket: 1n, value: 2, filteringId: -1n});`
                     }));

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-protected-audience');
  assert_equals(reports, null);

  // We use a short timeout as the previous poll should've waited long enough.
  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience',
      /*wait_for=*/ 1, /*timeout=*/ 50)
  assert_equals(debug_reports, null);
}, 'auction that calls Private Aggregation with negative filtering ID');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2, filteringId: 1n });
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2, filteringId: 2n });`
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
          MULTIPLE_CONTRIBUTIONS_DIFFERING_IN_FILTERING_ID_EXAMPLE,
          NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-protected-audience');
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
}, 'auction that calls Private Aggregation with contributions that match buckets but not filtering IDs');
