const NULL_CONTRIBUTION = Object.freeze({
  bucket: encodeBigInt(0n, 16),
  value: encodeBigInt(0n, 4),
  id: encodeBigInt(0n, 1),
});

/**
 * Returns a frozen payload object suitable for comparison with an object
 * returned by `CborParser.parse()`. The returned payload's `data` field is a
 * shallow copy of the `contributions` array, padded with `nullContribution`
 * until there are `padToNumContributions` elements.
 */
function buildExpectedPayload(
    contributions, padToNumContributions,
    nullContribution = NULL_CONTRIBUTION) {
  assert_less_than_equal(
      contributions.length, padToNumContributions,
      `Must have fewer than ${padToNumContributions} contributions`);

  const numNulls = padToNumContributions - contributions.length;
  const contributionsPadded =
      [...contributions].concat(Array(numNulls).fill(nullContribution));

  return Object.freeze({
    operation: 'histogram',
    data: contributionsPadded,
  });
}

/**
 * Asserts that the given payloads are equal by value. Provides more legible
 * error messages than `assert_object_equals()`.
 */
function assert_payload_equals(actualPayload, expectedPayload) {
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
        `Expected contribution at index ${i} has the wrong number of keys: ${
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

/**
 * Returns a `Uint8Array` containing the base64-decoded bytes of `data_base64`.
 * Throws an exception when the input is not valid base64.
 */
function decodeBase64ToUint8Array(data_base64) {
  // In JavaScript, strings are sequences of UTF-16 code units. The `atob()`
  // function returns returns a string where each code unit lies between 0
  // and 255 inclusive. Thus, filling a `Uint8Array` by mapping over the
  // code units with `charCodeAt(0)` will not invoke any text encoding
  // malarkey or produce any out-of-range values. While `TextEncoder`
  // sometimes does what we want, it *will* fail on values > 0x7f, which are
  // non-ASCII characters.
  return Uint8Array.from(atob(data_base64), c => c.charCodeAt(0));
}

class CborParser {
  /**
   * Returns a JavaScript object parsed from `data_base64`, which should be a
   * base64-encoded string containing CBOR-encoded data. Asserts that the input
   * is well-formed and is completely understood by the parser. This is an
   * ad-hoc parser; it knows just enough about CBOR to parse payloads destined
   * for the Aggregation Service.
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
