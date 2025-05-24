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

  const data = {
    enableDebugMode: true,
    contributions: [
      {event: 'non-reserved-event', bucket: 3n, value: 4},
      {bucket: 1n, value: 2},
    ]
  };
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  // No report is expected as the non-reserved event should throw an exception.
  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'run() that calls Private Aggregation with a non-reserved event');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    enableDebugMode: true,
    contributions: [
      {event: 'reserved.non-existent', bucket: 3n, value: 4},
      {bucket: 1n, value: 2},
    ]
  };
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage',
      /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE));

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'run() that calls Private Aggregation with a non-reserved event should no-op');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    enableDebugMode: true,
    contributions: [
      {event: 'reserved.report-success', bucket: 'not a big int', value: 4},
      {bucket: 1n, value: 2},
    ]
  };
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  // No report is expected as the invalid contribution should throw an
  // exception.
  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'run() that calls Private Aggregation with an invalid contribution');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    enableDebugMode: true,
    contributions: [
      {event: 'reserved.report-success', bucket: 1n, value: 2},
      {bucket: 3n, value: 4},

      // Other events should not be triggered.
      {event: 'reserved.too-many-contributions', bucket: 4n, value: 5},
      {event: 'reserved.empty-report-dropped', bucket: 5n, value: 6},
      {event: 'reserved.pending-report-limit-reached', bucket: 6n, value: 7},
      {event: 'reserved.insufficient-budget', bucket: 7n, value: 8},
      {event: 'reserved.uncaught-error', bucket: 8n, value: 9},
      {event: 'reserved.contribution-timeout-reached', bucket: 9n, value: 10},
    ]
  };
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage',
      /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          MULTIPLE_CONTRIBUTIONS_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE));

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'run() that calls Private Aggregation and triggers report-success');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  let contributions =
      buildArrayOfSequentialContributions(NUM_CONTRIBUTIONS_SHARED_STORAGE + 2);
  contributions[0].event = 'reserved.too-many-contributions';

  const data = {
    enableDebugMode: true,
    contributions: [
      ...contributions,

      // Other events should not be triggered.
      {event: 'reserved.report-success', bucket: 4n, value: 5},
      {event: 'reserved.empty-report-dropped', bucket: 5n, value: 6},
      {event: 'reserved.pending-report-limit-reached', bucket: 6n, value: 7},
      {event: 'reserved.insufficient-budget', bucket: 7n, value: 8},
      {event: 'reserved.uncaught-error', bucket: 8n, value: 9},
      {event: 'reserved.contribution-timeout-reached', bucket: 9n, value: 10},
    ]
  };
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage',
      /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildPayloadWithSequentialContributions(
          NUM_CONTRIBUTIONS_SHARED_STORAGE));

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'run() that calls Private Aggregation and triggers too-many-contributions');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    enableDebugMode: true,
    contributions: [
      {event: 'reserved.empty-report-dropped', bucket: 1n, value: 2},

      // Other events should not be triggered.
      {event: 'reserved.report-success', bucket: 4n, value: 5},
      {event: 'reserved.too-many-contributions', bucket: 5n, value: 6},
      {event: 'reserved.pending-report-limit-reached', bucket: 6n, value: 7},
      {event: 'reserved.insufficient-budget', bucket: 7n, value: 8},
      {event: 'reserved.uncaught-error', bucket: 8n, value: 9},
      {event: 'reserved.contribution-timeout-reached', bucket: 9n, value: 10},
    ]
  };
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage',
      /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE));

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'run() that calls Private Aggregation and triggers empty-report-dropped');


private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    enableDebugMode: true,
    contributions: [
      {event: 'reserved.insufficient-budget', bucket: 1n, value: 2},
      {bucket: 3n, value: 4},
      {bucket: 3n, value: 2 ** 16 - 3},

      // Other events should not be triggered.
      {event: 'reserved.report-success', bucket: 4n, value: 5},
      {event: 'reserved.too-many-contributions', bucket: 5n, value: 6},
      {event: 'reserved.empty-report-dropped', bucket: 6n, value: 7},
      {event: 'reserved.pending-report-limit-reached', bucket: 7n, value: 8},
      {event: 'reserved.uncaught-error', bucket: 8n, value: 9},
      {event: 'reserved.contribution-timeout-reached', bucket: 9n, value: 10},
    ]
  };
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage',
      /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          MULTIPLE_CONTRIBUTIONS_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE));

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'run() that calls Private Aggregation and triggers insufficient-budget');


private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    enableDebugMode: true,
    exceptionToThrow: new Error('example error'),
    contributions: [
      {event: 'reserved.uncaught-error', bucket: 1n, value: 2},
      {bucket: 3n, value: 4},

      // Other events should not be triggered, except report-success.
      {event: 'reserved.too-many-contributions', bucket: 5n, value: 6},
      {event: 'reserved.empty-report-dropped', bucket: 6n, value: 7},
      {event: 'reserved.pending-report-limit-reached', bucket: 7n, value: 8},
      {event: 'reserved.insufficient-budget', bucket: 8n, value: 9},
      {event: 'reserved.contribution-timeout-reached', bucket: 9n, value: 10},
    ]
  };
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage',
      /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          MULTIPLE_CONTRIBUTIONS_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE));

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'run() that calls Private Aggregation and triggers uncaught-error');
