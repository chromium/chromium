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
    contributions: [{bucket: 1n, value: 2, filteringId: 16777215n}]
  };
  await sharedStorage.run('contribute-to-histogram', {
    data,
    keepAlive: true,
    privateAggregationConfig: {filteringIdMaxBytes: 3}
  });

  // Expect a null report because we used a non-default `filteringIdMaxBytes`
  // and only made contributions the browser will reject.
  const {reports: [report]} = await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ false,
      /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined,
      /*expected_context_id=*/ undefined);
}, 'run() that calls Private Aggregation with max filtering ID for custom max bytes');


private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    contributions: [{bucket: 1n, value: 2, filteringId: 16777216n}],
    enableDebugMode: false
  };
  await sharedStorage.run('contribute-to-histogram', {
    data,
    keepAlive: true,
    privateAggregationConfig: {filteringIdMaxBytes: 3}
  });

  // Expect a null report because we used a non-default `filteringIdMaxBytes`
  // and only made contributions the browser will reject.
  const {reports: [report]} = await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ false,
      /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined,
      /*expected_context_id=*/ undefined);
}, 'run() that calls Private Aggregation with too big filtering ID for custom max bytes');


private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    contributions: [{bucket: 1n, value: 2, filteringId: (1n << 64n)}],
    enableDebugMode: false
  };
  await sharedStorage.run('contribute-to-histogram', {
    data,
    keepAlive: true,
    privateAggregationConfig: {filteringIdMaxBytes: 3}
  });

  // Expect a null report because we used a non-default `filteringIdMaxBytes`
  // and only made contributions the browser will reject.
  const {reports: [report]} = await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ false,
      /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined,
      /*expected_context_id=*/ undefined);
}, 'run() that calls Private Aggregation with too big filtering ID for largest max bytes possible');


private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {contributions: [], enableDebugMode: false};
  await sharedStorage.run('contribute-to-histogram', {
    data,
    keepAlive: true,
    privateAggregationConfig: {filteringIdMaxBytes: 3}
  });

  // Expect a null report because we used a non-default `filteringIdMaxBytes`
  // and made no contributions.
  const {reports: [report]} = await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);
  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ false,
      /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined,
      /*expected_context_id=*/ undefined);
}, 'run() that calls Private Aggregation with a non-default filtering ID max bytes and no contributions');
