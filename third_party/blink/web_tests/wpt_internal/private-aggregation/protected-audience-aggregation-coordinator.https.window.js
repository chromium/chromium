// META: timeout=long
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=../aggregation-service/support/aggregation-service.js
// META: script=resources/utils.js
// META: script=/private-aggregation/resources/protected-audience-helper-module.js

'use strict';

const reportPoller = new ReportPoller(
    '/.well-known/private-aggregation/report-protected-audience',
    '/.well-known/private-aggregation/debug/report-protected-audience',
    /*fullTimeoutMs=*/ 5000,
);

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: get_host_info().HTTPS_REMOTE_ORIGIN
  };

  await runReportTest(
      test, uuid, /*codeToInsert=*/ {
        generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
      },
      /*expectedNumReports=*/ 0,
      /*overrides=*/ {joinAdInterestGroup: {privateAggregationConfig}});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE),
      /*context_id=*/ undefined,
      /*aggregation_coordinator_origin=*/ get_host_info().HTTPS_REMOTE_ORIGIN);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'using Private Aggregation in generateBid with an allowed non-default coordinator');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: get_host_info().HTTPS_ORIGIN
  };

  await runReportTest(
      test, uuid, /*codeToInsert=*/ {
        generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
      },
      /*expectedNumReports=*/ 0,
      /*overrides=*/ {joinAdInterestGroup: {privateAggregationConfig}});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE),
      /*context_id=*/ undefined,
      /*aggregation_coordinator_origin=*/ get_host_info().HTTPS_ORIGIN);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'using Private Aggregation in generateBid with the default coordinator');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: get_host_info().HTTPS_REMOTE_ORIGIN
  };

  await runReportTest(
      test, uuid, /*codeToInsert=*/ {
        scoreAd: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
      },
      /*expectedNumReports=*/ 0,
      /*overrides=*/ {runAdAuction: {privateAggregationConfig}});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE),
      /*context_id=*/ undefined,
      /*aggregation_coordinator_origin=*/ get_host_info().HTTPS_REMOTE_ORIGIN);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'using Private Aggregation in scoreAd with an allowed non-default coordinator');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: get_host_info().HTTPS_ORIGIN
  };

  await runReportTest(
      test, uuid, /*codeToInsert=*/ {
        scoreAd: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
      },
      /*expectedNumReports=*/ 0,
      /*overrides=*/ {runAdAuction: {privateAggregationConfig}});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE),
      /*context_id=*/ undefined,
      /*aggregation_coordinator_origin=*/ get_host_info().HTTPS_ORIGIN);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'using Private Aggregation in scoreAd with the default coordinator');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: get_host_info().HTTPS_REMOTE_ORIGIN
  };

  await runReportTest(
      test, uuid, /*codeToInsert=*/ {
        reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
      },
      /*expectedNumReports=*/ 0,
      /*overrides=*/ {joinAdInterestGroup: {privateAggregationConfig}});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE),
      /*context_id=*/ undefined,
      /*aggregation_coordinator_origin=*/ get_host_info().HTTPS_REMOTE_ORIGIN);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'using Private Aggregation in reportWin affected by coordinator choice');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: get_host_info().HTTPS_REMOTE_ORIGIN
  };

  await runReportTest(
      test, uuid, /*codeToInsert=*/ {
        reportResult: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
      },
      /*expectedNumReports=*/ 0,
      /*overrides=*/ {runAdAuction: {privateAggregationConfig}});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE),
      /*context_id=*/ undefined,
      /*aggregation_coordinator_origin=*/ get_host_info().HTTPS_REMOTE_ORIGIN);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'using Private Aggregation in reportResult affected by coordinator choice');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: get_host_info().HTTPS_REMOTE_ORIGIN
  };

  await runReportTest(
      test, uuid, /*codeToInsert=*/ {
        reportWin: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
      },
      /*expectedNumReports=*/ 0,
      /*overrides=*/ {runAdAuction: {privateAggregationConfig}});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE),
      /*context_id=*/ undefined,
      /*aggregation_coordinator_origin=*/ get_host_info().HTTPS_ORIGIN);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'using Private Aggregation in reportWin NOT affected by sellers\'s coordinator choice');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: get_host_info().HTTPS_REMOTE_ORIGIN
  };

  await runReportTest(
      test, uuid, /*codeToInsert=*/ {
        reportResult: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
      },
      /*expectedNumReports=*/ 0,
      /*overrides=*/ {joinAdInterestGroup: {privateAggregationConfig}});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'protected-audience',
      /*is_debug_enabled=*/ true, /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE),
      /*context_id=*/ undefined,
      /*aggregation_coordinator_origin=*/ get_host_info().HTTPS_ORIGIN);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'using Private Aggregation in reportResult NOT affected by bidder\'s coordinator choice');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: get_host_info().HTTPS_REMOTE_ORIGIN
  };

  await runReportTest(
      test, uuid, /*codeToInsert=*/ {
        generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`,
        scoreAd: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
      },
      /*expectedNumReports=*/ 0, /*overrides=*/ {
        joinAdInterestGroup: {privateAggregationConfig},
        runAdAuction: {privateAggregationConfig},
      });

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'using Private Aggregation in bidder and seller, batched together when same origin and same coordinator');

private_aggregation_promise_test(async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: get_host_info().HTTPS_REMOTE_ORIGIN
  };

  await runReportTest(
      test, uuid, /*codeToInsert=*/ {
        generateBid: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`,
        scoreAd: `privateAggregation.enableDebugMode();
        privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
      },
      /*expectedNumReports=*/ 0,
      /*overrides=*/ {joinAdInterestGroup: {privateAggregationConfig}});

  // We don't verify the reports as they could arrive in a different order.
  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 2, /*expectedNumDebugReports=*/ 2);
}, 'using Private Aggregation in bidder and seller, NOT batched together when same origin and different coordinator');
