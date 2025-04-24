// META: variant=?include=callFromEachWorkletFunction
// META: variant=?include=batching
// META: variant=?include=contributionMerging
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
    'callFromEachWorkletFunction',
    private_aggregation_promise_test, async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
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
    }, 'auction that calls Private Aggregation in generateBid');

subsetTestByKey(
    'callFromEachWorkletFunction',
    private_aggregation_promise_test, async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        scoreAd: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
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
    }, 'auction that calls Private Aggregation in scoreAd');

subsetTestByKey(
    'callFromEachWorkletFunction',
    private_aggregation_promise_test, async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
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
    }, 'auction that calls Private Aggregation in reportWin');

subsetTestByKey(
    'callFromEachWorkletFunction',
    private_aggregation_promise_test, async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        reportResult: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
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
    }, 'auction that calls Private Aggregation in reportResult');

subsetTestByKey('batching', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    reportResult: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`
  });

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          MULTIPLE_CONTRIBUTIONS_EXAMPLE,
          NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'auction that calls Private Aggregation with multiple contributions');

subsetTestByKey('batching', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`,
    reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`
  });

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          MULTIPLE_CONTRIBUTIONS_EXAMPLE,
          NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'auction that calls Private Aggregation batches across different functions');

subsetTestByKey('batching', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    reportResult:
        `privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
  });

  const {reports: [report]} = await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);
  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ false, /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined);
}, 'auction that calls Private Aggregation without debug mode');

subsetTestByKey('batching', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    reportResult: `privateAggregation.enableDebugMode({ debugKey: 1234n });
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
  });

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
}, 'auction that calls Private Aggregation using a debug key');

subsetTestByKey('batching', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode({ debugKey: 1234n });
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`,
    reportWin: `privateAggregation.enableDebugMode({ debugKey: 2345n });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`
  });

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 2, /*expectedNumDebugReports=*/ 2);
}, 'auction that calls Private Aggregation does not batch different functions if debug keys differ');

subsetTestByKey('batching', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid:
        `privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`,
    reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`
  });

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 2, /*expectedNumDebugReports=*/ 1);
}, 'auction that calls Private Aggregation does not batch different functions if debug modes differ');

subsetTestByKey('batching', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`,
    reportWin: `privateAggregation.enableDebugMode({ debugKey: 1234n });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`
  });

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 2, /*expectedNumDebugReports=*/ 2);
}, 'auction that calls Private Aggregation does not batch different functions if debug key presence differs');

subsetTestByKey('batching', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
  });

  await runReportTest(test, uuid, {
    reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });`
  });

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 2, /*expectedNumDebugReports=*/ 2);
}, 'auctions that call Private Aggregation do not batch across different auctions');

subsetTestByKey('batching', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();
  await runReportTest(test, uuid, {
    reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });
        aNonExistentVariable;`
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
}, 'auction that calls Private Aggregation then has unrelated error still sends reports');

subsetTestByKey(
    'contributionMerging', private_aggregation_promise_test, async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 3 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 1 });`
      });

      const {reports: [report], debug_reports: [debug_report]} =
          await reportPoller.pollReportsAndAssert(
              /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);
      verifyReport(
          report, /*api=*/ 'protected-audience',
          /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
          /*expected_payload=*/
          buildExpectedPayload(
              MULTIPLE_CONTRIBUTIONS_EXAMPLE,
              NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

      verifyReportsIdenticalExceptPayload(report, debug_report);
    }, 'auction that calls Private Aggregation with mergeable contributions');

subsetTestByKey(
    'contributionMerging', private_aggregation_promise_test, async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 0 });`
      });

      const {reports: [report], debug_reports: [debug_report]} =
          await reportPoller.pollReportsAndAssert(
              /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);
      verifyReport(
          report, /*api=*/ 'protected-audience',
          /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
          /*expected_payload=*/
          buildExpectedPayload(
              MULTIPLE_CONTRIBUTIONS_EXAMPLE,
              NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

      verifyReportsIdenticalExceptPayload(report, debug_report);
    }, 'auction that calls Private Aggregation with zero-value contributions');

subsetTestByKey(
    'contributionMerging', private_aggregation_promise_test, async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.enableDebugMode();
        for (let i = 1n; i <= ${
            NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE +
            1}n; i++) {  // Too many contributions
          privateAggregation.contributeToHistogram({ bucket: i, value: 1 });
        }`
      });

      const {reports: [report], debug_reports: [debug_report]} =
          await reportPoller.pollReportsAndAssert(
              /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);
      verifyReport(
          report, /*api=*/ 'protected-audience',
          /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
          /*expected_payload=*/
          buildPayloadWithSequentialContributions(
              NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

      verifyReportsIdenticalExceptPayload(report, debug_report);
    }, 'auction that calls Private Aggregation with too many contributions');

subsetTestByKey(
    'contributionMerging', private_aggregation_promise_test,
    async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.enableDebugMode();
        // Too many contributions ignoring merging
        for (let i = 1n; i <= 21n; i++) {
          privateAggregation.contributeToHistogram({ bucket: 1n, value: 1 });
        }`
      });

      const {reports: [report], debug_reports: [debug_report]} =
          await reportPoller.pollReportsAndAssert(
              /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);
      verifyReport(
          report, /*api=*/ 'protected-audience',
          /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
          /*expected_payload=*/
          buildExpectedPayload(
              ONE_CONTRIBUTION_HIGHER_VALUE_EXAMPLE,
              NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE));

      verifyReportsIdenticalExceptPayload(report, debug_report);
    },
    'auction that calls Private Aggregation with many mergeable contributions');

subsetTestByKey(
    'contributionMerging', private_aggregation_promise_test,
    async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.enableDebugMode();
        // Sums to value 1 if overflow is allowed.
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2147483647 });
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 1073741824 });
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 1073741824 });
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
      });

      // The final contribution should succeed, but the first three should fail.
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
    'auction that calls Private Aggregation with values that sum to more than the max long');
