// META: variant=?include=validFilteringId
// META: variant=?include=invalidFilteringId
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

subsetTestByKey(
    'validFilteringId', private_aggregation_promise_test,
    async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram(
            {bucket: 1n, value: 2, filteringId: 3n});`
      });

      const {reports: [report], debug_reports: [debug_report]} =
          await reportPoller.pollReportsAndAssert(
              /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

      verifyReport(
          report, /*api=*/ 'protected-audience',
          /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
          /*expected_payload=*/
          buildExpectedPayload(
              ONE_CONTRIBUTION_WITH_FILTERING_ID_EXAMPLE,
              NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

      verifyReportsIdenticalExceptPayload(report, debug_report);
    },
    'auction that calls Private Aggregation with a non-default filtering ID');

subsetTestByKey(
    'validFilteringId', private_aggregation_promise_test,
    async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.contributeToHistogram(
            {bucket: 1n, value: 2, filteringId: 3n});`
      });

      const {reports: [report]} = await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);

      verifyReport(
          report, /*api=*/ 'protected-audience',
          /*is_debug_enabled=*/ false, /*debug_key=*/ undefined,
          /*expected_payload=*/ undefined);
    },
    'auction that calls Private Aggregation with a non-default filtering ID and no debug mode');

subsetTestByKey(
    'validFilteringId', private_aggregation_promise_test, async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({bucket: 1n, value: 2});`
      });

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
    }, 'auction that calls Private Aggregation with no filtering ID specified');


subsetTestByKey(
    'validFilteringId', private_aggregation_promise_test,
    async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram(
            {bucket: 1n, value: 2, filteringId: 0n});`
      });

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
    },
    'auction that calls Private Aggregation with an explicitly default filtering ID');


subsetTestByKey(
    'validFilteringId', private_aggregation_promise_test, async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.contributeToHistogram(
                      {bucket: 1n, value: 2, filteringId: 255n});`
      });

      const {reports: [report]} = await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);

      verifyReport(
          report, /*api=*/ 'protected-audience',
          /*is_debug_enabled=*/ false, /*debug_key=*/ undefined,
          /*expected_payload=*/ undefined);
    }, 'auction that calls Private Aggregation with max filtering ID');


subsetTestByKey(
    'invalidFilteringId', private_aggregation_promise_test, async test => {
      const uuid = generateUuid();

      promise_rejects_js(
          test, TypeError, runReportTest(test, uuid, {
            generateBid: `privateAggregation.contributeToHistogram(
                          {bucket: 1n, value: 2, filteringId: 256n});`
          }));

      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
    }, 'auction that calls Private Aggregation with filtering ID too big');

subsetTestByKey(
    'invalidFilteringId', private_aggregation_promise_test, async test => {
      const uuid = generateUuid();
      promise_rejects_js(
          test, TypeError, runReportTest(test, uuid, {
            generateBid: `privateAggregation.contributeToHistogram(
                          {bucket: 1n, value: 2, filteringId: -1n});`
          }));

      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
    }, 'auction that calls Private Aggregation with negative filtering ID');

subsetTestByKey('validFilteringId', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2, filteringId: 1n });
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2, filteringId: 2n });`
  });

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          MULTIPLE_CONTRIBUTIONS_DIFFERING_IN_FILTERING_ID_EXAMPLE,
          NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'auction that calls Private Aggregation with contributions that match buckets but not filtering IDs');
