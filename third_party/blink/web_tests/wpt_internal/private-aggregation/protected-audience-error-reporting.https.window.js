// META: variant=?include=basicErrorReporting
// META: variant=?include=moreErrorReporting
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
    'basicErrorReporting', private_aggregation_promise_test, async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.report-success', { bucket: 1n, value: 2 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });

        // Other events should not be triggered.
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.too-many-contributions', { bucket: 4n, value: 5 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.empty-report-dropped', { bucket: 5n, value: 6 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.pending-report-limit-reached', { bucket: 6n, value: 7 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.insufficient-budget', { bucket: 7n, value: 8 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.uncaught-error', { bucket: 8n, value: 9 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.contribution-timeout-reached', { bucket: 9n, value: 10 });`
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
    }, 'auction that calls Private Aggregation and triggers report-success');

subsetTestByKey(
    'basicErrorReporting', private_aggregation_promise_test,
    async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        reportResult: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.too-many-contributions', { bucket: 1n, value: 1 });
        for (let i = 2n; i <= ${
            NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE +
            2}n; i++) {  // Too many contributions
          privateAggregation.contributeToHistogram({ bucket: i, value: 1 });
        }

        // Other events should not be triggered.
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.report-success', { bucket: 4n, value: 5 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.empty-report-dropped', { bucket: 5n, value: 6 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.pending-report-limit-reached', { bucket: 6n, value: 7 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.insufficient-budget', { bucket: 7n, value: 8 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.uncaught-error', { bucket: 8n, value: 9 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.contribution-timeout-reached', { bucket: 9n, value: 10 });`
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
    },
    'auction that calls Private Aggregation and triggers too-many-contributions');

subsetTestByKey(
    'basicErrorReporting', private_aggregation_promise_test,
    async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        reportResult: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.insufficient-budget', { bucket: 1n, value: 2 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 2 ** 16 - 3 });

        // Other events should not be triggered.
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.report-success', { bucket: 4n, value: 5 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.too-many-contributions', { bucket: 5n, value: 6 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.pending-report-limit-reached', { bucket: 6n, value: 7 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.empty-report-dropped', { bucket: 7n, value: 8 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.uncaught-error', { bucket: 8n, value: 9 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.contribution-timeout-reached', { bucket: 9n, value: 10 });`
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
    },
    'auction that calls Private Aggregation and triggers insufficient-budget');

subsetTestByKey(
    'basicErrorReporting', private_aggregation_promise_test,
    async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        reportResult: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.empty-report-dropped', { bucket: 1n, value: 2 });

        // Other events should not be triggered.
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.report-success', { bucket: 4n, value: 5 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.too-many-contributions', { bucket: 5n, value: 6 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.insufficient-budget', { bucket: 6n, value: 7 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.pending-report-limit-reached', { bucket: 7n, value: 8 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.uncaught-error', { bucket: 8n, value: 9 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.contribution-timeout-reached', { bucket: 9n, value: 10 });`
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
    'auction that calls Private Aggregation and triggers empty-report-dropped');

subsetTestByKey(
    'basicErrorReporting', private_aggregation_promise_test, async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        reportResult: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.uncaught-error', { bucket: 1n, value: 2 });
        privateAggregation.contributeToHistogram({ bucket: 3n, value: 4 });

        // Other events should not be triggered.
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.too-many-contributions', { bucket: 5n, value: 6 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.empty-report-dropped', { bucket: 6n, value: 7 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.pending-report-limit-reached', { bucket: 7n, value: 8 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.insufficient-budget', { bucket: 8n, value: 9 });
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.contribution-timeout-reached', { bucket: 9n, value: 10 });

        throw new Error('example error');`
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
    }, 'auction that calls Private Aggregation and triggers uncaught-error');

subsetTestByKey(
    'moreErrorReporting', private_aggregation_promise_test,
    async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.enableDebugMode();
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.non-existent', { bucket: 3n, value: 4 });
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
    },
    'auction that calls Private Aggregation with an unknown reserved event should no-op');

subsetTestByKey(
    'moreErrorReporting', private_aggregation_promise_test,
    async test => {
      const uuid = generateUuid();
      await runReportTest(test, uuid, {
        scoreAd: `privateAggregation.enableDebugMode();
      for (let i = 2n; i <= ${
            NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE +
            2}n; i++) {  // Too many contributions
        privateAggregation.contributeToHistogram({ bucket: i, value: 1 });
      }`,
        reportResult: `privateAggregation.enableDebugMode();
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.too-many-contributions', { bucket: 1n, value: 1 });

      // Other events should not be triggered.
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.report-success', { bucket: 4n, value: 5 });
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.empty-report-dropped', { bucket: 5n, value: 6 });
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.pending-report-limit-reached', { bucket: 6n, value: 7 });
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.insufficient-budget', { bucket: 7n, value: 8 });
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.uncaught-error', { bucket: 8n, value: 9 });
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.contribution-timeout-reached', { bucket: 9n, value: 10 });`
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
    },
    'auction that calls Private Aggregation and triggers too-many-contributions from a different function call');

subsetTestByKey(
    'moreErrorReporting', private_aggregation_promise_test,
    async test => {
      const uuid = generateUuid();

      await joinInterestGroup(test, uuid, {name: '1'});
      await joinInterestGroup(test, uuid, {name: '2'});

      await runReportTest(test, uuid, {
        generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.empty-report-dropped', { bucket: 1n, value: 2 });`,
        scoreAd: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogramOnEvent(
            'reserved.insufficient-budget', { bucket: 3n, value: 4 });
        privateAggregation.contributeToHistogram({ bucket: 0n, value: 2**16 });`
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
    },
    'auction that calls Private Aggregation and triggers error reporting from multiple IGs only triggers one error');
