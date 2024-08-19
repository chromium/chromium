/**
 * Returns a `Uint8Array` of `size` bytes containing the big-endian
 * representation of the `BigInt` parameter `num`. Asserts that `num` can be
 * represented in `size` bytes.
 */
function encodeBigInt(num, size) {
  assert_equals(num.constructor.name, 'BigInt');
  const numOriginal = num;

  const array = Array(size).fill(0);
  for (let i = 0; i < size; ++i) {
    array[size - i - 1] = Number(num % 256n);
    num >>= 8n;
  }
  assert_equals(
      num, 0n,
      `encodeBigInt must encode ${numOriginal} in ${
          size} bytes with a remainder of zero`);
  return Uint8Array.from(array);
}

// The default null contribution with a default filtering ID.
const NULL_CONTRIBUTION = Object.freeze({
  bucket: encodeBigInt(0n, 16),
  value: encodeBigInt(0n, 4),
  id: encodeBigInt(0n, 1),
});

// A variant of the default null contribution with a filteringIdMaxBytes of 3.
const NULL_CONTRIBUTION_WITH_CUSTOM_FILTERING_ID_MAX_BYTES = Object.freeze({
  bucket: encodeBigInt(0n, 16),
  value: encodeBigInt(0n, 4),
  id: encodeBigInt(0n, 3),
});

// A single contribution {bucket: 1n, value: 2}, with default filtering ID
const ONE_CONTRIBUTION_EXAMPLE = Object.freeze({
  operation: 'histogram',
  data: [{
    bucket: encodeBigInt(1n, 16),
    value: encodeBigInt(2n, 4),
    id: encodeBigInt(0n, 1),
  }],
});

// Contributions [{bucket: 1n, value: 2}, {bucket: 3n, value: 4}] with default
// filtering IDs
const MULTIPLE_CONTRIBUTIONS_EXAMPLE = Object.freeze({
  operation: 'histogram',
  data: [
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
  ]
});

// A single contribution {bucket: 1n, value: 2, filteringId: 3n}]
const ONE_CONTRIBUTION_WITH_FILTERING_ID_EXAMPLE = Object.freeze({
  operation: 'histogram',
  data: [
    {
      bucket: encodeBigInt(1n, 16),
      value: encodeBigInt(2n, 4),
      id: encodeBigInt(3n, 1),
    },
  ]
});

// A single contribution {bucket: 1n, value: 2} using a filteringIdMaxBytes of
// 3.
const ONE_CONTRIBUTION_WITH_CUSTOM_FILTERING_ID_MAX_BYTES_EXAMPLE =
    Object.freeze({
      operation: 'histogram',
      data: [
        {
          bucket: encodeBigInt(1n, 16),
          value: encodeBigInt(2n, 4),
          id: encodeBigInt(0n, 3),
        },
      ]
    });

// A single contribution {bucket: 1n, value: 2, filteringId: 259n} using a
// filteringIdMaxBytes of 3.
const ONE_CONTRIBUTION_WITH_FILTERING_ID_AND_CUSTOM_MAX_BYTES_EXAMPLE =
    Object.freeze({
      operation: 'histogram',
      data: [
        {
          bucket: encodeBigInt(1n, 16),
          value: encodeBigInt(2n, 4),
          id: encodeBigInt(259n, 3),
        },
      ]
    });

// Contributions [{bucket: 1n, value: 2, filteringId: 1n},
// {bucket: 1n, value: 2, filteringId: 2n}]
const MULTIPLE_CONTRIBUTIONS_DIFFERING_IN_FILTERING_ID_EXAMPLE = Object.freeze({
  operation: 'histogram',
  data: [
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
  ]
});

// Contributions [{bucket: i, value: 1} for i from 1 to 20, inclusive.
const CONTRIBUTIONS_UP_TO_LIMIT_EXAMPLE = Object.freeze({
  operation: 'histogram',
  data: Array(20).fill().map((_, i) => ({
                               bucket: encodeBigInt(BigInt(i + 1), 16),
                               value: encodeBigInt(1n, 4),
                               id: encodeBigInt(0n, 1),
                             })),
});

// A single contribution {bucket: 1n, value: 21}
const ONE_CONTRIBUTION_HIGHER_VALUE_EXAMPLE = Object.freeze({
  operation: 'histogram',
  data: [
    {
      bucket: encodeBigInt(1n, 16),
      value: encodeBigInt(21n, 4),
      id: encodeBigInt(0n, 1),
    },
  ]
});

/**
 * Returns a `Uint8Array` containing the base64-decoded bytes of `data_base64`.
 * Throws an exception when the input is not valid base64.
 */
const decodeBase64ToUint8Array = (data_base64) => {
  // In JavaScript, strings are sequences of UTF-16 code units. The `atob()`
  // function returns returns a string where each code unit lies between 0
  // and 255 inclusive. Thus, filling a `Uint8Array` by mapping over the
  // code units with `charCodeAt(0)` will not invoke any text encoding
  // malarkey or produce any out-of-range values. While `TextEncoder`
  // sometimes does what we want, it *will* fail on values > 0x7f, which are
  // non-ASCII characters.
  return Uint8Array.from(atob(data_base64), c => c.charCodeAt(0));
};

class CborParser {
  /**
   * Returns a JavaScript object parsed from `data_base64`, which should be a
   * base64-encoded string containing CBOR-encoded data. Asserts that the input
   * is well-formed and is completely understood by the parser. This is an
   * ad-hoc parser; it knows just enough about CBOR to parse Private Aggregation
   * payloads.
   */
  static parse(data_base64) {
    const bytes = decodeBase64ToUint8Array(data_base64);
    const parser = new CborParser(bytes);
    const object = parser.#parseCbor();
    assert_equals(
        parser.#bytes.length, 0,
        'CborParser did not consume trailing bytes:\n' +
            CborParser.#hexdump(parser.#bytes));
    return object;
  }

  #bytes

  constructor(bytes) {
    assert_equals(bytes.constructor.name, 'Uint8Array');
    this.#bytes = bytes;
  }

  // Reads the "initial byte" from `this.data` and recursively parses the data
  // item it indicates. This is based on the RFC8949 definition of CBOR, which
  // is fully backwards compatible with RFC7049.
  //
  // For reference, see RFC8949's Appendix B: Jump Table For Initial Byte:
  // https://www.rfc-editor.org/rfc/rfc8949.html#section-appendix.b
  #parseCbor() {
    const [b] = this.#consume(1);

    if (0x40 <= b && b <= 0x57) {  // Parse a byte string.
      return this.#consume(b - 0x40);
    }

    if (0x60 <= b && b <= 0x77) {  // Parse a UTF-8 string.
      const str = this.#consume(b - 0x60);
      return (new TextDecoder()).decode(str);
    }

    if (0x80 <= b && b <= 0x97) {
      return this.#parseCborArray(b - 0x80);
    }
    if (b === 0x98) {
      return this.#parseCborArray(this.#parseUnsignedNum(1));
    }
    if (b === 0x99) {
      return this.#parseCborArray(this.#parseUnsignedNum(2));
    }
    if (b === 0x9a) {
      return this.#parseCborArray(this.#parseUnsignedNum(4));
    }

    if (0xa0 <= b && b <= 0xb7) {
      return this.#parseCborMap(b - 0xa0);
    }

    assert_unreached(`Unsupported initial byte 0x${b.toString(16)}`);
  }

  #parseUnsignedNum(len) {
    const bytes = this.#consume(len);
    return bytes.reduce((acc, byte) => acc << 8 | byte, 0);
  }

  #parseCborArray(len) {
    const out = [];
    for (let i = 0; i < len; ++i) {
      out.push(this.#parseCbor());
    }
    return out;
  }

  #parseCborMap(len) {
    const out = {};
    for (let i = 0; i < len; ++i) {
      const key = this.#parseCbor();
      const value = this.#parseCbor();
      out[key] = value;
    }
    return out;
  }

  #consume(len) {
    assert_less_than_equal(len, this.#bytes.length);
    const prefix = this.#bytes.slice(0, len);
    this.#bytes = this.#bytes.slice(len);
    return prefix;
  }

  static #hexdump(bytes) {
    assert_equals(bytes.constructor.name, 'Uint8Array');
    const WIDTH = 40;
    let dump = '';
    for (let i = 0; i < bytes.length; i += WIDTH) {
      const chunk = bytes.slice(i, i + WIDTH);
      const line = chunk.reduce(
          (acc, byte) => acc + byte.toString(16).padStart(2, '0'), '');
      dump += line + '\n';
    }
    return dump;
  }
}

/**
 * Asserts that the given payloads are equal by value. Provides more legible
 * error messages than `assert_object_equals()`.
 */
const assert_payload_equals =
    (actualPayload, expectedPayload) => {
      assert_equals(
          actualPayload.operation, expectedPayload.operation,
          'Payloads should have the same operation.');
      assert_equals(
          actualPayload.data.length, expectedPayload.data.length,
          'Payloads should have the same number of contributions.');

      assert_equals(
          Object.keys(actualPayload).length, 2,
          `actualPayload has the wrong number of keys: ${
              Object.keys(actualPayload)}`);
      assert_equals(
          Object.keys(expectedPayload).length, 2,
          `expectedPayload has the wrong number of keys: ${
              Object.keys(expectedPayload)}`);

      for (let i = 0; i < actualPayload.data.length; ++i) {
        const actualContribution = actualPayload.data[i];
        const expectedContribution = expectedPayload.data[i];

        assert_equals(
            Object.keys(actualContribution).length, 3,
            `Contribution at index ${i} has the wrong number of keys: ${
                Object.keys(actualContribution)}`);

        assert_equals(
            Object.keys(expectedContribution).length, 3,
            `Expected contribution at index ${
                i} has the wrong number of keys: ${
                Object.keys(expectedContribution)}`);

        for (let key of ['bucket', 'value', 'id']) {
          const actual = actualContribution[key];
          const expected = expectedContribution[key];
          assert_array_equals(
              actual, expected,
              `Contribution at index ${i} should have expected ${key}.`);
        }
      }
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
 * Verifies that a report's aggregation_service_payloads has the expected
 * fields. The `expected_payload` should be undefined if debug mode is disabled.
 * Otherwise, it should be the expected list of CBOR-encoded contributions in
 * the debug_cleartext_payload. `pad_with_contribution` is the contribution to
 * pad the payload with; if undefined is used, default padding will be used.
 */
const verifyAggregationServicePayloads =
    (aggregation_service_payloads, expected_payload, pad_with_contribution) => {
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
        verifyCleartextPayload(
            payload_obj.debug_cleartext_payload, expected_payload,
            pad_with_contribution);
      }

      // Check there are no extra keys
      assert_equals(Object.keys(payload_obj).length, expected_payload ? 3 : 2);
    };

/**
 * Verifies that a report's debug_cleartext_payload has the expected fields. The
 * `expected_payload` should be the expected list of CBOR-encoded contributions
 * in the debug_cleartext_payload.
 */
const verifyCleartextPayload =
    (debug_cleartext_payload, expected_payload,
     pad_with_contribution = NULL_CONTRIBUTION) => {
      const payload = CborParser.parse(debug_cleartext_payload);

      // Pad the payload.
      const num_null_contributions = 20 - expected_payload.data.length;
      const expected_payload_padded = {...expected_payload};
      expected_payload_padded.data = expected_payload_padded.data.concat(
          Array(num_null_contributions).fill(pad_with_contribution));

      // TODO(alexmt): Consider sorting both arguments in order to ignore
      // ordering.
      assert_payload_equals(payload, expected_payload_padded);
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
     aggregation_coordinator_origin = get_host_info().HTTPS_ORIGIN,
     pad_with_contribution = undefined) => {
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
          report.aggregation_service_payloads, expected_payload,
          pad_with_contribution);

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
