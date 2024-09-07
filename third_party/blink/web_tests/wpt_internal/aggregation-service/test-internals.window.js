// META: script=../aggregation-service/support/aggregation-service.js

test(() => {
  // The `TextEncoder` approach to converting strings to Uint8Array is tempting
  // because it sometimes works. ASCII values (0 through 0x7f) pass through
  // unmodified, but values between 0x80 and 0xff incorrectly produce multiple
  // bytes!
  assert_array_equals(decodeBase64ToUint8Array(btoa('\x7f')), [0x7f]);
  assert_array_equals(decodeBase64ToUint8Array(btoa('\x80')), [0x80]);
}, 'decodeBase64ToUint8Array() handles non-ASCII bytes');

test(() => {
  const ONE_CONTRIBUTION_EXAMPLE = Object.freeze([{
    bucket: encodeBigInt(1n, 16),
    value: encodeBigInt(2n, 4),
    id: encodeBigInt(0n, 1),
  }]);

  assert_payload_equals(
      buildExpectedPayload(ONE_CONTRIBUTION_EXAMPLE, 20),
      buildExpectedPayload(ONE_CONTRIBUTION_EXAMPLE, 20));
}, 'assert_payload_equals() is reflexive');

test(() => {
  // Payload with contributions [{bucket: 1n, value: 2, filteringId: 3n}],
  // padded to 20 contributions
  const ONE_CONTRIBUTION_PADDED =
      'omRkYXRhlKNiaWRBA2V2YWx1ZUQAAAACZmJ1Y2tldFAAAAAAAAAAAAAAAAAAAAABo2JpZEEAZX' +
      'ZhbHVlRAAAAABmYnVja2V0UAAAAAAAAAAAAAAAAAAAAACjYmlkQQBldmFsdWVEAAAAAGZidWNr' +
      'ZXRQAAAAAAAAAAAAAAAAAAAAAKNiaWRBAGV2YWx1ZUQAAAAAZmJ1Y2tldFAAAAAAAAAAAAAAAA' +
      'AAAAAAo2JpZEEAZXZhbHVlRAAAAABmYnVja2V0UAAAAAAAAAAAAAAAAAAAAACjYmlkQQBldmFs' +
      'dWVEAAAAAGZidWNrZXRQAAAAAAAAAAAAAAAAAAAAAKNiaWRBAGV2YWx1ZUQAAAAAZmJ1Y2tldF' +
      'AAAAAAAAAAAAAAAAAAAAAAo2JpZEEAZXZhbHVlRAAAAABmYnVja2V0UAAAAAAAAAAAAAAAAAAA' +
      'AACjYmlkQQBldmFsdWVEAAAAAGZidWNrZXRQAAAAAAAAAAAAAAAAAAAAAKNiaWRBAGV2YWx1ZU' +
      'QAAAAAZmJ1Y2tldFAAAAAAAAAAAAAAAAAAAAAAo2JpZEEAZXZhbHVlRAAAAABmYnVja2V0UAAA' +
      'AAAAAAAAAAAAAAAAAACjYmlkQQBldmFsdWVEAAAAAGZidWNrZXRQAAAAAAAAAAAAAAAAAAAAAK' +
      'NiaWRBAGV2YWx1ZUQAAAAAZmJ1Y2tldFAAAAAAAAAAAAAAAAAAAAAAo2JpZEEAZXZhbHVlRAAA' +
      'AABmYnVja2V0UAAAAAAAAAAAAAAAAAAAAACjYmlkQQBldmFsdWVEAAAAAGZidWNrZXRQAAAAAA' +
      'AAAAAAAAAAAAAAAKNiaWRBAGV2YWx1ZUQAAAAAZmJ1Y2tldFAAAAAAAAAAAAAAAAAAAAAAo2Jp' +
      'ZEEAZXZhbHVlRAAAAABmYnVja2V0UAAAAAAAAAAAAAAAAAAAAACjYmlkQQBldmFsdWVEAAAAAG' +
      'ZidWNrZXRQAAAAAAAAAAAAAAAAAAAAAKNiaWRBAGV2YWx1ZUQAAAAAZmJ1Y2tldFAAAAAAAAAA' +
      'AAAAAAAAAAAAo2JpZEEAZXZhbHVlRAAAAABmYnVja2V0UAAAAAAAAAAAAAAAAAAAAABpb3Blcm' +
      'F0aW9uaWhpc3RvZ3JhbQ==';

  const payload = CborParser.parse(ONE_CONTRIBUTION_PADDED);

  const EXPECTED_EXAMPLE = {
    operation: 'histogram',
    data: [{
            bucket: encodeBigInt(1n, 16),
            value: encodeBigInt(2n, 4),
            id: encodeBigInt(3n, 1),
          }].concat(Array(19).fill(NULL_CONTRIBUTION)),
  };

  assert_payload_equals(payload, EXPECTED_EXAMPLE);
}, 'CborParser correctly parses a realistic input');

test(() => {
  // Hex bytes of a payload with contributions [{bucket: 1n, value:2, id: 3n}].
  const ONE_CONTRIBUTION_HEX = [
    0xa2,                                                  // map(2)
    0x69,                                                  // text(9)
    0x6f, 0x70, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e,  // "operation"
    0x69,                                                  // text(9)
    0x68, 0x69, 0x73, 0x74, 0x6f, 0x67, 0x72, 0x61, 0x6d,  // "histogram"
    0x64,                                                  // text(4)
    0x64, 0x61, 0x74, 0x61,                                // "data"
    0x81,                                                  // array(1)
    0xa3,                                                  // map(3)
    0x62,                                                  // text(2)
    0x69, 0x64,                                            // "id"
    0x41,                                                  // bytes(1)
    0x03,                                                  //
    0x65,                                                  // text(5)
    0x76, 0x61, 0x6c, 0x75, 0x65,                          // "value"
    0x44,                                                  // bytes(4)
    0x00, 0x00, 0x00, 0x02,                                //
    0x66,                                                  // text(6)
    0x62, 0x75, 0x63, 0x6b, 0x65, 0x74,                    // "bucket"
    0x50,                                                  // bytes(16)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
  ];

  const payloadString = String.fromCharCode(...ONE_CONTRIBUTION_HEX);
  const payloadStringWithExtraByte =
      String.fromCharCode(...ONE_CONTRIBUTION_HEX, 0x42);

  // This should succeed without throwing an exception.
  const parsedPayload = CborParser.parse(btoa(payloadString));
  assert_payload_equals(parsedPayload, {
    operation: 'histogram',
    data: [{
      bucket: encodeBigInt(1n, 16),
      value: encodeBigInt(2n, 4),
      id: encodeBigInt(3n, 1),
    }],
  });

  // NOTE: We expect this to fail. See the expectation file that corresponds to
  // this source file.
  CborParser.parse(btoa(payloadStringWithExtraByte));
}, 'CborParser rejects input with a trailing, unparsed byte');

test(
    () =>
        assert_throws_dom('InvalidCharacterError', () => CborParser.parse('*')),
    'CborParser.parse rejects invalid base64 strings');

test(() => {
  assert_array_equals(
      CborParser.parse(btoa('\x98\x02\x65hello\x65world')), ['hello', 'world'],
      'Should parse CBOR array with one-byte length prefix');
  assert_array_equals(
      CborParser.parse(btoa('\x99\x00\x02\x65hello\x65world')),
      ['hello', 'world'],
      'Should parse CBOR array with two-byte length prefix');
  assert_array_equals(
      CborParser.parse(btoa('\x9a\x00\x00\x00\x02\x65hello\x65world')),
      ['hello', 'world'],
      'Should parse CBOR array with four-byte length prefix');
}, 'CborParser can parse arrays with length prefixes');
