// META: variant=?include=validOrigin
// META: variant=?include=invalidOrigin
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

subsetTestByKey('validOrigin', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: 'https://valid-but-not-allowed-origin.example'
  };

  promise_rejects_dom(
      test, 'DataError',
      runReportTest(
          test, uuid, /*codeToInsert=*/ {
            generateBid: `privateAggregation.enableDebugMode();
            privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
          },
          /*expectedNumReports=*/ 0,
          /*overrides=*/ {joinAdInterestGroup: {privateAggregationConfig}}));

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'using Private Aggregation in generateBid with an aggregationCoordinatorOrigin that is a valid origin but not on the allowlist');

subsetTestByKey('invalidOrigin', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: 'not-a-origin'
  };

  promise_rejects_dom(
      test, 'SyntaxError',
      runReportTest(
          test, uuid, /*codeToInsert=*/ {
            generateBid: `privateAggregation.enableDebugMode();
            privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
          },
          /*expectedNumReports=*/ 0,
          /*overrides=*/ {joinAdInterestGroup: {privateAggregationConfig}}));

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'using Private Aggregation in generateBid with with an aggregationCoordinatorOrigin that is not a valid origin');

subsetTestByKey('validOrigin', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: 'https://valid-but-not-allowed-origin.example'
  };

  promise_rejects_dom(
      test, 'DataError',
      runReportTest(
          test, uuid, /*codeToInsert=*/ {
            scoreAd: `privateAggregation.enableDebugMode();
            privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
          },
          /*expectedNumReports=*/ 0,
          /*overrides=*/ {runAdAuction: {privateAggregationConfig}}));

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'using Private Aggregation in scoreAd with an aggregationCoordinatorOrigin that is a valid origin but not on the allowlist');

subsetTestByKey('invalidOrigin', private_aggregation_promise_test, async test => {
  const uuid = generateUuid();

  const privateAggregationConfig = {
    aggregationCoordinatorOrigin: 'not-a-origin'
  };

  promise_rejects_dom(
      test, 'SyntaxError',
      runReportTest(
          test, uuid, /*codeToInsert=*/ {
            scoreAd: `privateAggregation.enableDebugMode();
            privateAggregation.contributeToHistogram({ bucket: 1n, value: 2 });`
          },
          /*expectedNumReports=*/ 0,
          /*overrides=*/ {runAdAuction: {privateAggregationConfig}}));

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'using Private Aggregation in scoreAd with with an aggregationCoordinatorOrigin that is not a valid origin');
