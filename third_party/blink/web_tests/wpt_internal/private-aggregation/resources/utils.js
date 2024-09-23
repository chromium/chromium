// Tests using this script should first import:
// ../../aggregation-service/support/aggregation-service.js

const NUM_CONTRIBUTIONS_SHARED_STORAGE = 20;
const NUM_CONTRIBUTIONS_PROTECTED_AUDIENCE = 20;

const NULL_CONTRIBUTION_WITH_CUSTOM_FILTERING_ID_MAX_BYTES = Object.freeze({
  bucket: encodeBigInt(0n, 16),
  value: encodeBigInt(0n, 4),
  id: encodeBigInt(0n, 3),
});

const ONE_CONTRIBUTION_EXAMPLE = Object.freeze([{
  bucket: encodeBigInt(1n, 16),
  value: encodeBigInt(2n, 4),
  id: encodeBigInt(0n, 1),
}]);

const MULTIPLE_CONTRIBUTIONS_EXAMPLE = Object.freeze([
  {
    bucket: encodeBigInt(1n, 16),
    value: encodeBigInt(2n, 4),
    id: encodeBigInt(0n, 1),
  },
  {
    bucket: encodeBigInt(3n, 16),
    value: encodeBigInt(4n, 4),
    id: encodeBigInt(0n, 1),
  },
]);

const ONE_CONTRIBUTION_WITH_FILTERING_ID_EXAMPLE = Object.freeze([
  {
    bucket: encodeBigInt(1n, 16),
    value: encodeBigInt(2n, 4),
    id: encodeBigInt(3n, 1),
  },
]);

const ONE_CONTRIBUTION_WITH_CUSTOM_FILTERING_ID_MAX_BYTES_EXAMPLE =
    Object.freeze([
      {
        bucket: encodeBigInt(1n, 16),
        value: encodeBigInt(2n, 4),
        id: encodeBigInt(0n, 3),
      },
    ]);

const ONE_CONTRIBUTION_WITH_FILTERING_ID_AND_CUSTOM_MAX_BYTES_EXAMPLE =
    Object.freeze([
      {
        bucket: encodeBigInt(1n, 16),
        value: encodeBigInt(2n, 4),
        id: encodeBigInt(259n, 3),
      },
    ]);

const MULTIPLE_CONTRIBUTIONS_DIFFERING_IN_FILTERING_ID_EXAMPLE = Object.freeze([
  {
    bucket: encodeBigInt(1n, 16),
    value: encodeBigInt(2n, 4),
    id: encodeBigInt(1n, 1),
  },
  {
    bucket: encodeBigInt(1n, 16),
    value: encodeBigInt(2n, 4),
    id: encodeBigInt(2n, 1),
  },
]);

const ONE_CONTRIBUTION_HIGHER_VALUE_EXAMPLE = Object.freeze([
  {
    bucket: encodeBigInt(1n, 16),
    value: encodeBigInt(21n, 4),
    id: encodeBigInt(0n, 1),
  },
]);

/**
 * Returns a frozen payload object with contributions of the form `{bucket: i,
 * value: 1}` for i from 1 to `numContributions`, inclusive.
 */
function buildPayloadWithSequentialContributions(numContributions) {
  return Object.freeze({
    operation: 'histogram',
    data: Array(numContributions).fill().map((_, i) => ({
                                               bucket: encodeBigInt(
                                                   BigInt(i) + 1n, 16),
                                               value: encodeBigInt(1n, 4),
                                               id: encodeBigInt(0n, 1),
                                             })),
  });
}

const private_aggregation_promise_test = (f, name) => promise_test(async t => {
  await resetWptServer();
  await f(t);
}, name);

const resetWptServer = () => Promise.all([
  resetReports(
      '/.well-known/private-aggregation/debug/report-protected-audience'),
  resetReports('/.well-known/private-aggregation/debug/report-shared-storage'),
  resetReports('/.well-known/private-aggregation/report-protected-audience'),
  resetReports('/.well-known/private-aggregation/report-shared-storage'),
]);

/**
 * Method to clear the stash. Takes the URL as parameter.
 */
const resetReports = url => {
  // The view of the stash is path-specific
  // (https://web-platform-tests.org/tools/wptserve/docs/stash.html), therefore
  // the origin doesn't need to be specified.
  url = `${url}?clear_stash=true`;
  const options = {
    method: 'POST',
  };
  return fetch(url, options);
};

/**
 * Delay method that waits for prescribed number of milliseconds.
 */
const delay = ms => new Promise(resolve => step_timeout(resolve, ms));

/**
 * Polls the given `url` to retrieve reports sent there. Once the reports are
 * received, returns the list of reports. Returns null if the timeout is reached
 * before a report is available.
 */
const pollReports = async (url, wait_for = 1, timeout = 5000 /*ms*/) => {
  let startTime = performance.now();
  let payloads = [];
  while (performance.now() - startTime < timeout) {
    const resp = await fetch(new URL(url, location.origin));
    const payload = await resp.json();
    if (payload.length > 0) {
      payloads = payloads.concat(payload);
    }
    if (payloads.length >= wait_for) {
      return payloads;
    }
    await delay(/*ms=*/ 100);
  }
  if (payloads.length > 0) {
    return payloads;
  }
  return null;
};

/**
 * Verifies that a report's shared_info string is serialized JSON with the
 * expected fields. `is_debug_enabled` should be a boolean corresponding to
 * whether debug mode is expected to be enabled for this report.
 */
const verifySharedInfo = (shared_info_str, api, is_debug_enabled) => {
  const shared_info = JSON.parse(shared_info_str);
  assert_equals(shared_info.api, api);
  if (is_debug_enabled) {
    assert_equals(shared_info.debug_mode, 'enabled');
  } else {
    assert_not_own_property(shared_info, 'debug_mode');
  }

  const uuid_regex =
      RegExp('^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$');
  assert_own_property(shared_info, 'report_id');
  assert_true(uuid_regex.test(shared_info.report_id));

  assert_equals(shared_info.reporting_origin, location.origin);

  // The amount of delay is implementation-defined.
  const integer_regex = RegExp('^[0-9]*$');
  assert_own_property(shared_info, 'scheduled_report_time');
  assert_true(integer_regex.test(shared_info.scheduled_report_time));

  assert_equals(shared_info.version, '1.0');

  // Check there are no extra keys
  assert_equals(Object.keys(shared_info).length, is_debug_enabled ? 6 : 5);
};

/**
 * Verifies that a report's aggregation_service_payloads has the expected
 * fields. The `expected_payload` should be undefined if debug mode is disabled.
 * Otherwise, it should be the expected list of CBOR-encoded contributions in
 * the debug_cleartext_payload. `pad_with_contribution` is the contribution to
 * pad the payload with; if undefined is used, default padding will be used.
 */
const verifyAggregationServicePayloads =
    (aggregation_service_payloads, expected_payload) => {
      assert_equals(aggregation_service_payloads.length, 1);
      const payload_obj = aggregation_service_payloads[0];

      assert_own_property(payload_obj, 'key_id');
      // The only id specified in the test key file.
      assert_equals(payload_obj.key_id, 'example_id');

      assert_own_property(payload_obj, 'payload');
      // Check the payload is base64 encoded. We do not decrypt the payload to
      // test its contents.
      atob(payload_obj.payload);

      if (expected_payload) {
        assert_own_property(payload_obj, 'debug_cleartext_payload');

        const payload = CborParser.parse(payload_obj.debug_cleartext_payload);

        // TODO(alexmt): Consider sorting both payloads in order to ignore
        // ordering.
        assert_payload_equals(payload, expected_payload);
      }

      // Check there are no extra keys
      assert_equals(Object.keys(payload_obj).length, expected_payload ? 3 : 2);
    };

/**
 * Verifies that a report has the expected fields. `is_debug_enabled` should be
 * a boolean corresponding to whether debug mode is expected to be enabled for
 * this report. `debug_key` should be the debug key if set; otherwise,
 * undefined. The `expected_payload` should be the expected value of
 * debug_cleartext_payload if debug mode is enabled; otherwise, undefined.
 */
const verifyReport =
    (report, api, is_debug_enabled, debug_key, expected_payload = undefined,
     context_id = undefined,
     aggregation_coordinator_origin = get_host_info().HTTPS_ORIGIN) => {
      if (debug_key || expected_payload) {
        // A debug key cannot be set without debug mode being enabled and the
        // `expected_payload` should be undefined if debug mode is not enabled.
        assert_true(is_debug_enabled);
      }

      assert_own_property(report, 'shared_info');
      verifySharedInfo(report.shared_info, api, is_debug_enabled);

      if (debug_key) {
        assert_own_property(report, 'debug_key');
        assert_equals(report.debug_key, debug_key);
      } else {
        assert_not_own_property(report, 'debug_key');
      }

      assert_own_property(report, 'aggregation_service_payloads');
      verifyAggregationServicePayloads(
          report.aggregation_service_payloads, expected_payload);

      assert_own_property(report, 'aggregation_coordinator_origin');
      assert_equals(
          report.aggregation_coordinator_origin,
          aggregation_coordinator_origin);

      if (context_id) {
        assert_own_property(report, 'context_id');
        assert_equals(report.context_id, context_id);
      } else {
        assert_not_own_property(report, 'context_id');
      }

      // Check there are no extra keys
      let expected_length = 3;
      if (debug_key) {
        ++expected_length;
      }
      if (context_id) {
        ++expected_length;
      }
      assert_equals(Object.keys(report).length, expected_length);
    };

/**
 * Verifies that two reports are identical except for the payload (which is
 * encrypted and thus non-deterministic). Assumes that reports are well formed,
 * so should only be called after verifyReport().
 */
const verifyReportsIdenticalExceptPayload = (report_a, report_b) => {
  report_a.aggregation_service_payloads[0].payload = 'PAYLOAD';
  report_b.aggregation_service_payloads[0].payload = 'PAYLOAD';

  assert_equals(JSON.stringify(report_a), JSON.stringify(report_b));
}
