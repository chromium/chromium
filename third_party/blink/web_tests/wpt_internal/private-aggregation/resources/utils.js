// The expected start and end of a cleartext payload, i.e. before and after the
// contributions themselves.
const EXPECTED_PAYLOAD_START_SEQUENCE = atob('omRkYXRhlA==');
const EXPECTED_PAYLOAD_END_SEQUENCE = atob('aW9wZXJhdGlvbmloaXN0b2dyYW0=');

// The 'empty' contribution used for padding with a default filteringIdMaxBytes
const PADDING_PAYLOAD_COMPONENT =
    atob('o2JpZEEAZXZhbHVlRAAAAABmYnVja2V0UAAAAAAAAAAAAAAAAAAAAAA=');

// The 'empty' contribution used for padding with a filteringIdMaxBytes of 3
const PADDING_PAYLOAD_COMPONENT_WITH_CUSTOM_MAX_BYTES =
    atob('o2JpZEMAAABldmFsdWVEAAAAAGZidWNrZXRQAAAAAAAAAAAAAAAAAAAAAA==');

// Pads an array to the given length.
const padContributions =
    (contributions, pad_with = PADDING_PAYLOAD_COMPONENT,
     length_to_pad_to = 20) => {
      assert_less_than_equal(contributions.length, length_to_pad_to);

      padded_contributions = [...contributions];
      for (let i = contributions.length; i < length_to_pad_to; i++) {
        padded_contributions.push(pad_with);
      }
      return padded_contributions;
    }

// A single contribution {bucket: 1n, value: 2}, with default filtering ID
const ONE_CONTRIBUTION_PAYLOAD_COMPONENT =
    atob('o2JpZEEAZXZhbHVlRAAAAAJmYnVja2V0UAAAAAAAAAAAAAAAAAAAAAE=');
const ONE_CONTRIBUTION_EXAMPLE_COMPONENTS =
    [ONE_CONTRIBUTION_PAYLOAD_COMPONENT];

// Contributions [{bucket: 1n, value: 2}, {bucket: 3n, value: 4}] with default
// filtering IDs
const MULTIPLE_CONTRIBUTIONS_EXAMPLE_COMPONENTS = [
  ONE_CONTRIBUTION_PAYLOAD_COMPONENT,
  atob('o2JpZEEAZXZhbHVlRAAAAARmYnVja2V0UAAAAAAAAAAAAAAAAAAAAAM=')
];

// A single contribution {bucket: 1n, value: 2, filteringId: 3n}]
const ONE_CONTRIBUTION_WITH_FILTERING_ID_EXAMPLE_COMPONENTS =
    [atob('o2JpZEEDZXZhbHVlRAAAAAJmYnVja2V0UAAAAAAAAAAAAAAAAAAAAAE=')];

// A single contribution {bucket: 1n, value: 2} using a filteringIdMaxBytes
// of 3.
const ONE_CONTRIBUTION_WITH_CUSTOM_FILTERING_ID_MAX_BYTES_EXAMPLE_COMPONENTS =
    [atob('o2JpZEMAAABldmFsdWVEAAAAAmZidWNrZXRQAAAAAAAAAAAAAAAAAAAAAQ==')];

// A single contribution {bucket: 1n, value: 2, filteringId: 259n} using a
// filteringIdMaxBytes of 3.
const
    ONE_CONTRIBUTION_WITH_FILTERING_ID_AND_CUSTOM_MAX_BYTES_EXAMPLE_COMPONENTS =
        [atob('o2JpZEMAAQNldmFsdWVEAAAAAmZidWNrZXRQAAAAAAAAAAAAAAAAAAAAAQ==')];

// Contributions [{bucket: 1n, value: 2, filteringId: 1n},
// {bucket: 1n, value: 2, filteringId: 2n}]
const MULTIPLE_CONTRIBUTIONS_DIFFERING_IN_FILTERING_ID_EXAMPLE_COMPONENTS = [
  atob('o2JpZEEBZXZhbHVlRAAAAAJmYnVja2V0UAAAAAAAAAAAAAAAAAAAAAE='),
  atob('o2JpZEECZXZhbHVlRAAAAAJmYnVja2V0UAAAAAAAAAAAAAAAAAAAAAE=')
];

// Contributions [{bucket: i, value: 1} for i from 1 to 20, inclusive.
const CONTRIBUTIONS_UP_TO_LIMIT_EXAMPLE_COMPONENTS = (() => {
  let contributions = [];
  let endings = [
    'AE=', 'AI=', 'AM=', 'AQ=', 'AU=', 'AY=', 'Ac=', 'Ag=', 'Ak=', 'Ao=',
    'As=', 'Aw=', 'A0=', 'A4=', 'A8=', 'BA=', 'BE=', 'BI=', 'BM=', 'BQ='
  ];
  for (let ending of endings) {
    contributions.push(
        atob('o2JpZEEAZXZhbHVlRAAAAAFmYnVja2V0UAAAAAAAAAAAAAAAAAAAA' + ending))
  }
  return contributions;
})();

// A single contribution {bucket: 1n, value: 21}
const ONE_CONTRIBUTION_HIGHER_VALUE_EXAMPLE_COMPONENTS =
    [atob('o2JpZEEAZXZhbHVlRAAAABVmYnVja2V0UAAAAAAAAAAAAAAAAAAAAAE=')];

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
  shared_info = JSON.parse(shared_info_str);
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
 * Verifies that an report's aggregation_service_payloads has the expected
 * fields. The `expected_contribution_payload_components` should be undefined if
 * debug mode is disabled. Otherwise, it should be the expected list of
 * CBOR-encoded contributions in the debug_cleartext_payload. `pad_with` is the
 * contribution to pad the payload with; if undefined is used, default padding
 * will be used.
 */
const verifyAggregationServicePayloads =
    (aggregation_service_payloads, expected_contribution_payload_components,
     pad_with) => {
      assert_equals(aggregation_service_payloads.length, 1);
      const payload_obj = aggregation_service_payloads[0];

      assert_own_property(payload_obj, 'key_id');
      // The only id specified in the test key file.
      assert_equals(payload_obj.key_id, 'example_id');

      assert_own_property(payload_obj, 'payload');
      // Check the payload is base64 encoded. We do not decrypt the payload to
      // test its contents.
      atob(payload_obj.payload);

      if (expected_contribution_payload_components) {
        assert_own_property(payload_obj, 'debug_cleartext_payload');
        verifyCleartextPayload(
            payload_obj.debug_cleartext_payload,
            expected_contribution_payload_components, pad_with);
      }

      // Check there are no extra keys
      assert_equals(
          Object.keys(payload_obj).length,
          expected_contribution_payload_components ? 3 : 2);
    };

/**
 * Verifies that an report's debug_cleartext_payload has the expected fields.
 * The `expected_contribution_payload_components` should be the expected list of
 * CBOR-encoded contributions in the debug_cleartext_payload.
 */
const verifyCleartextPayload =
    (debug_cleartext_payload, expected_contribution_payload_components,
     pad_with) => {
      expected_padded_payload_components =
          padContributions(expected_contribution_payload_components, pad_with);

      // The text encoder is used to convert strings into an array of bytes to
      // avoid issues like multi-byte characters reducing the apparent length.
      const text_encoder = new TextEncoder();
      for (let i = 0; i < expected_padded_payload_components.length; i++) {
        expected_padded_payload_components[i] =
            text_encoder.encode(expected_padded_payload_components[i]);
      }

      const decoded_payload =
          text_encoder.encode(atob(debug_cleartext_payload));

      // Check beginning and end of the payload (i.e. before and after the
      // contribution components)
      const expected_start_seq =
          text_encoder.encode(EXPECTED_PAYLOAD_START_SEQUENCE);
      const expected_end_seq =
          text_encoder.encode(EXPECTED_PAYLOAD_END_SEQUENCE);
      assert_array_equals(
          decoded_payload.slice(0, expected_start_seq.length),
          expected_start_seq);
      assert_array_equals(
          decoded_payload.slice(-expected_end_seq.length), expected_end_seq);

      // Check the rest is a valid ordering of the components.
      const rest_of_payload = decoded_payload.slice(
          expected_start_seq.length, -expected_end_seq.length);

      assert_true(expected_padded_payload_components.length > 1);
      const payload_contribution_length =
          expected_padded_payload_components[0].length;

      // All expected contributions should have the same length.
      for (let expected_payload_contribution of
               expected_padded_payload_components) {
        assert_equals(
            expected_payload_contribution.length, payload_contribution_length);
      }

      assert_equals(
          rest_of_payload.length,
          payload_contribution_length *
              expected_padded_payload_components.length);

      let payload_contributions = [];
      for (let i = 0; i < expected_padded_payload_components.length; i++) {
        const payload_contribution = rest_of_payload.slice(
            i * payload_contribution_length,
            (i + 1) * payload_contribution_length);
        payload_contributions.push(payload_contribution);
      }

      // TODO(alexmt): Consider sorting both arguments in order to ignore
      // ordering.
      assert_equals(
          expected_padded_payload_components.length,
          payload_contributions.length);
      for (let i = 0; i < payload_contributions.length; i++) {
        assert_array_equals(
            expected_padded_payload_components[i], payload_contributions[i]);
      }
    };

/**
 * Verifies that an report has the expected fields. `is_debug_enabled` should be
 * a boolean corresponding to whether debug mode is expected to be enabled for
 * this report. `debug_key` should be the debug key if set; otherwise,
 * undefined. The `expected_contribution_payload_components` should be the
 * expected value of debug_cleartext_payload if debug mode is enabled;
 * otherwise, undefined.
 */
const verifyReport =
    (report, api, is_debug_enabled, debug_key,
     expected_contribution_payload_components, context_id = undefined,
     aggregation_coordinator_origin = get_host_info().HTTPS_ORIGIN,
     pad_with = undefined) => {
      if (debug_key || expected_contribution_payload_components) {
        // A debug key cannot be set without debug mode being enabled and the
        // `expected_contribution_payload_components` should be undefined if
        // debug mode is not enabled.
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
          report.aggregation_service_payloads,
          expected_contribution_payload_components, pad_with);

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
