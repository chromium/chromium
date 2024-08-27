// META: timeout=long
// META: script=/common/get-host-info.sub.js
// META: script=../aggregation-service/support/aggregation-service.js
// META: script=resources/utils.js
// META: script=/shared-storage/resources/util.js

'use strict';

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {enableDebugMode: true, contributions: [{bucket: 1n, value: 2}]};
  await sharedStorage.run('contribute-to-histogram', {
    data,
    keepAlive: true,
    privateAggregationConfig: {contextId: 'example-context-id'}
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-shared-storage',
      /*wait_for=*/ 1, /*timeout=*/ 6000)
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE),
      /*expected_context_id=*/ 'example-context-id');

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-shared-storage')
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
}, 'run() that calls Private Aggregation with context ID');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    enableDebugMode: false,
    contributions: [{bucket: 1n, value: 2}]
  };
  await sharedStorage.run('contribute-to-histogram', {
    data,
    keepAlive: true,
    privateAggregationConfig: {contextId: 'example-context-id'}
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-shared-storage',
      /*wait_for=*/ 1, /*timeout=*/ 6000)
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ false,
      /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined,
      /*expected_context_id=*/ 'example-context-id');

  // We use a short timeout as the previous poll should've waited long enough.
  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-shared-storage',
      /*wait_for=*/ 1, /*timeout=*/ 50)
  assert_equals(debug_reports, null);
}, 'run() that calls Private Aggregation with context ID and no debug mode');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {enableDebugMode: false, contributions: []};
  await sharedStorage.run('contribute-to-histogram', {
    data,
    keepAlive: true,
    privateAggregationConfig: {contextId: 'example-context-id'}
  });

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-shared-storage',
      /*wait_for=*/ 1, /*timeout=*/ 6000)
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ false,
      /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined,
      /*expected_context_id=*/ 'example-context-id');

  // We use a short timeout as the previous poll should've waited long enough.
  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-shared-storage',
      /*wait_for=*/ 1, /*timeout=*/ 50)
  assert_equals(debug_reports, null);
}, 'run() that calls Private Aggregation with context ID and no contributions');
