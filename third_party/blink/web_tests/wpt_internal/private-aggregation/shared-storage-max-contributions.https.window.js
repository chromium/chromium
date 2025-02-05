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

const DEBUG_MODE_TEST_CASES = [
  {
    label: 'Default number of contributions when maxContributions is omitted',
    contributions: [{bucket: 1n, value: 2}],
    maxContributions: undefined,
    expectedPayload: buildExpectedPayload(
        ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE),
  },
  {
    label: 'Use maxContributions to get fewer contributions than default',
    contributions: [{bucket: 1n, value: 2}],
    maxContributions: NUM_CONTRIBUTIONS_SHARED_STORAGE - 1,
    expectedPayload: buildExpectedPayload(
        ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE - 1),
  },
  {
    label: 'Use maxContributions to get more contributions than default',
    contributions: [{bucket: 1n, value: 2}],
    maxContributions: NUM_CONTRIBUTIONS_SHARED_STORAGE + 1,
    expectedPayload: buildExpectedPayload(
        ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE + 1),
  },
  {
    label: 'Contributions in excess of maxContributions are truncated',
    contributions:
        buildArrayOfSequentialContributions(NUM_CONTRIBUTIONS_SHARED_STORAGE),
    maxContributions: NUM_CONTRIBUTIONS_SHARED_STORAGE - 1,
    expectedPayload: buildPayloadWithSequentialContributions(
        NUM_CONTRIBUTIONS_SHARED_STORAGE - 1),
  },
];

for (const testCase of DEBUG_MODE_TEST_CASES) {
  private_aggregation_promise_test(async () => {
    await addModuleOnce('resources/shared-storage-module.js');

    await sharedStorage.run('contribute-to-histogram', {
      data: {
        contributions: testCase.contributions,
        enableDebugMode: true,
      },
      keepAlive: true,
      privateAggregationConfig: {
        maxContributions: testCase.maxContributions,
      }
    });

    const {reports: [report], debug_reports: [debugReport]} =
        await reportPoller.pollReportsAndAssert(
            /*expectedNumReports=*/ 1,
            /*expectedNumDebugReports=*/ 1);

    verifyReport(
        report, /*api=*/ 'shared-storage',
        /*is_debug_enabled=*/ true,
        /*debug_key=*/ undefined, testCase.expectedPayload,
        /*expected_context_id=*/ undefined);

    verifyReportsIdenticalExceptPayload(report, debugReport);
  }, testCase.label);
}

const NULL_REPORT_TEST_CASES = [
  {
    label: 'Null report sent when maxContributions is less than default',
    contributions: [],
    maxContributions: NUM_CONTRIBUTIONS_SHARED_STORAGE + 1,
    expectNullReport: true,
  },
  {
    label: 'Null report sent when maxContributions is greater than default',
    contributions: [],
    maxContributions: NUM_CONTRIBUTIONS_SHARED_STORAGE - 1,
    expectNullReport: true,
  },
  {
    label: 'No null report sent when maxContributions is equal to default',
    contributions: [],
    maxContributions: NUM_CONTRIBUTIONS_SHARED_STORAGE,
    expectNullReport: false,
  },
];

for (const testCase of NULL_REPORT_TEST_CASES) {
  private_aggregation_promise_test(async () => {
    await addModuleOnce('resources/shared-storage-module.js');

    await sharedStorage.run('contribute-to-histogram', {
      data: {contributions: testCase.contributions},
      keepAlive: true,
      privateAggregationConfig: {maxContributions: testCase.maxContributions},
    });

    if (!testCase.expectNullReport) {
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 0,
          /*expectedNumDebugReports=*/ 0);
      return;
    }

    const {reports: [report]} = await reportPoller.pollReportsAndAssert(
        /*expectedNumReports=*/ 1,
        /*expectedNumDebugReports=*/ 0);

    verifyReport(
        report, /*api=*/ 'shared-storage',
        /*is_debug_enabled=*/ false,
        /*debug_key=*/ undefined);
  }, testCase.label);
}

private_aggregation_promise_test(async (t) => {
  await addModuleOnce('resources/shared-storage-module.js');

  promise_rejects_dom(
      t, 'DataError', sharedStorage.run('contribute-to-histogram', {
        data: {contributions: ONE_CONTRIBUTION_EXAMPLE},
        keepAlive: true,
        privateAggregationConfig: {maxContributions: 0}
      }));

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'run() throws exception when maxContributions is zero');

private_aggregation_promise_test(async (t) => {
  await addModuleOnce('resources/shared-storage-module.js');

  promise_rejects_js(
      t, TypeError, sharedStorage.run('contribute-to-histogram', {
        data: {contributions: []},
        keepAlive: true,
        privateAggregationConfig: {maxContributions: Infinity}
      }));

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'run() throws exception when maxContributions is not an unsigned long long');
