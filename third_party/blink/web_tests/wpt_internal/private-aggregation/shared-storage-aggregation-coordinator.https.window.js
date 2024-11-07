// META: timeout=long
// META: script=/common/get-host-info.sub.js
// META: script=../aggregation-service/support/aggregation-service.js
// META: script=resources/utils.js
// META: script=/shared-storage/resources/util.js

'use strict';

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: get_host_info().HTTPS_REMOTE_ORIGIN
  };
  const data = {enableDebugMode: true, contributions: [{bucket: 1n, value: 2}]};

  await sharedStorage.run(
      'contribute-to-histogram',
      {data, keepAlive: true, privateAggregationConfig});


  const reports = await pollReports(
      '/.well-known/private-aggregation/report-shared-storage')
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE),
      /*context_id=*/ undefined,
      /*aggregation_coordinator_origin=*/ get_host_info().HTTPS_REMOTE_ORIGIN);

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-shared-storage')
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
}, 'run() that calls Private Aggregation with an allowed, non-default aggregationCoordinatorOrigin');


private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: get_host_info().HTTPS_ORIGIN
  };
  const data = {enableDebugMode: true, contributions: [{bucket: 1n, value: 2}]};

  await sharedStorage.run(
      'contribute-to-histogram',
      {data, keepAlive: true, privateAggregationConfig});


  const reports = await pollReports(
      '/.well-known/private-aggregation/report-shared-storage')
  assert_equals(reports.length, 1);

  const report = JSON.parse(reports[0]);
  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE),
      /*context_id=*/ undefined,
      /*aggregation_coordinator_origin=*/ get_host_info().HTTPS_ORIGIN);

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-shared-storage')
  assert_equals(debug_reports.length, 1);

  verifyReportsIdenticalExceptPayload(report, JSON.parse(debug_reports[0]));
}, 'run() that calls Private Aggregation with explicitly specifying the default aggregationCoordinatorOrigin');

private_aggregation_promise_test(async (test) => {
  await addModuleOnce('resources/shared-storage-module.js');

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: 'https://valid-but-not-allowed-origin.example'
  };

  const data = {enableDebugMode: true, contributions: [{bucket: 1n, value: 2}]};
  promise_rejects_dom(
      test, 'DataError',
      sharedStorage.run(
          'contribute-to-histogram',
          {data, keepAlive: true, privateAggregationConfig}));

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-shared-storage')
  assert_equals(reports, null);

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-shared-storage',
      /*wait_for=*/ 1, /*timeout=*/ 50);
  assert_equals(debug_reports, null);
}, 'run() that calls Private Aggregation with an aggregationCoordinatorOrigin that is a valid origin but not on the allowlist');

private_aggregation_promise_test(async (test) => {
  await addModuleOnce('resources/shared-storage-module.js');

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: 'not-a-origin'
  };

  const data = {enableDebugMode: true, contributions: [{bucket: 1n, value: 2}]};
  promise_rejects_dom(
      test, 'SyntaxError',
      sharedStorage.run(
          'contribute-to-histogram',
          {data, keepAlive: true, privateAggregationConfig}));

  const reports = await pollReports(
      '/.well-known/private-aggregation/report-shared-storage')
  assert_equals(reports, null);

  const debug_reports = await pollReports(
      '/.well-known/private-aggregation/debug/report-shared-storage',
      /*wait_for=*/ 1, /*timeout=*/ 50);
  assert_equals(debug_reports, null);
}, 'run() that calls Private Aggregation with an aggregationCoordinatorOrigin that is not a valid origin');
