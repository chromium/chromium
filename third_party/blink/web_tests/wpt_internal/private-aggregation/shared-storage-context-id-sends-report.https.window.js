// META: timeout=long
// META: script=/common/get-host-info.sub.js
// META: script=../aggregation-service/support/aggregation-service.js
// META: script=resources/utils.js
// META: script=/shared-storage/resources/util.js

'use strict';

const reportPoller = new ReportPoller(
    '/.well-known/private-aggregation/report-shared-storage',
    '/.well-known/private-aggregation/debug/report-shared-storage',
    /*fullTimeoutMs=*/ 2000,
);

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {enableDebugMode: true, contributions: [{bucket: 1n, value: 2}]};
  await sharedStorage.run('contribute-to-histogram', {
    data,
    keepAlive: true,
    privateAggregationConfig: {contextId: 'example-context-id'}
  });

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE),
      /*expected_context_id=*/ 'example-context-id');

  verifyReportsIdenticalExceptPayload(report, debug_report);
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

  const {reports: [report]} = await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ false,
      /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined,
      /*expected_context_id=*/ 'example-context-id');
}, 'run() that calls Private Aggregation with context ID and no debug mode');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {enableDebugMode: false, contributions: []};
  await sharedStorage.run('contribute-to-histogram', {
    data,
    keepAlive: true,
    privateAggregationConfig: {contextId: 'example-context-id'}
  });

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ false,
      /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined,
      /*expected_context_id=*/ 'example-context-id');
}, 'run() that calls Private Aggregation with context ID and no contributions');
