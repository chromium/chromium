/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for TextFormatSerializer.
 */

/** @suppress {extraProvide} */
goog.module('goog.proto2.TextFormatSerializerTest');
goog.setTestOnly();

const ObjectSerializer = goog.require('goog.proto2.ObjectSerializer');
const TestAllTypes = goog.require('proto2.TestAllTypes');
const TextFormatSerializer = goog.require('goog.proto2.TextFormatSerializer');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * Asserts that the given string value parses into the given set of tokens.
 * @param {string} value The string value to parse.
 * @param {Array<Object> | Object} tokens The tokens to check against. If not an
 *     array, a single token is expected.
 * @param {boolean=} ignoreWhitespace Whether whitespace tokens should be
 *     skipped by the tokenizer.
 */
function assertTokens(value, tokens, ignoreWhitespace = undefined) {
  /** @suppress {visibility} suppression added to enable type checking */
  const tokenizer =
      new TextFormatSerializer.Tokenizer_(value, ignoreWhitespace);
  const tokensFound = [];

  while (tokenizer.next()) {
    tokensFound.push(tokenizer.getCurrent());
  }

  if (!Array.isArray(tokens)) {
    tokens = [tokens];
  }

  assertEquals(tokens.length, tokensFound.length);
  for (let i = 0; i < tokens.length; ++i) {
    assertToken(tokens[i], tokensFound[i]);
  }
}

function assertToken(expected, found) {
  assertEquals(expected.type, found.type);
  if (expected.value) {
    assertEquals(expected.value, found.value);
  }
}

function assertIdentifier(identifier) {
  const types = TextFormatSerializer.Tokenizer_.TokenTypes;
  assertTokens(identifier, {type: types.IDENTIFIER, value: identifier});
}

function assertComment(comment) {
  const types = TextFormatSerializer.Tokenizer_.TokenTypes;
  assertTokens(comment, {type: types.COMMENT, value: comment});
}

function assertString(str) {
  const types = TextFormatSerializer.Tokenizer_.TokenTypes;
  assertTokens(str, {type: types.STRING, value: str});
}

function assertNumber(num) {
  num = num.toString();
  const types = TextFormatSerializer.Tokenizer_.TokenTypes;
  assertTokens(num, {type: types.NUMBER, value: num});
}

const floatFormatCases = [
  {given: '1.69e+06', expect: 1.69e+06},
  {given: '1.69e6', expect: 1.69e+06},
  {given: '2.468e-2', expect: 0.02468},
];

testSuite({
  testSerialization() {
    const message = new TestAllTypes();

    // Set the fields.
    // Singular.
    message.setOptionalInt32(101);
    message.setOptionalUint32(103);
    message.setOptionalSint32(105);
    message.setOptionalFixed32(107);
    message.setOptionalSfixed32(109);
    message.setOptionalInt64('102');
    message.setOptionalFloat(111.5);
    message.setOptionalDouble(112.5);
    message.setOptionalBool(true);
    message.setOptionalString('test');
    message.setOptionalBytes('abcd');

    const group = new TestAllTypes.OptionalGroup();
    group.setA(111);

    message.setOptionalgroup(group);

    const nestedMessage = new TestAllTypes.NestedMessage();
    nestedMessage.setB(112);

    message.setOptionalNestedMessage(nestedMessage);

    message.setOptionalNestedEnum(TestAllTypes.NestedEnum.FOO);

    // Repeated.
    message.addRepeatedInt32(201);
    message.addRepeatedInt32(202);

    // Serialize to a simplified text format.
    const simplified = new TextFormatSerializer().serialize(message);
    const expected = 'optional_int32: 101\n' +
        'optional_int64: 102\n' +
        'optional_uint32: 103\n' +
        'optional_sint32: 105\n' +
        'optional_fixed32: 107\n' +
        'optional_sfixed32: 109\n' +
        'optional_float: 111.5\n' +
        'optional_double: 112.5\n' +
        'optional_bool: true\n' +
        'optional_string: "test"\n' +
        'optional_bytes: "abcd"\n' +
        'optionalgroup {\n' +
        '  a: 111\n' +
        '}\n' +
        'optional_nested_message {\n' +
        '  b: 112\n' +
        '}\n' +
        'optional_nested_enum: FOO\n' +
        'repeated_int32: 201\n' +
        'repeated_int32: 202\n';

    assertEquals(expected, simplified);
  },

  testSerializationOfUnknown() {
    const nestedUnknown = new TestAllTypes();
    const message = new TestAllTypes();

    // Set the fields.
    // Known.
    message.setOptionalInt32(101);
    message.addRepeatedInt32(201);
    message.addRepeatedInt32(202);

    nestedUnknown.addRepeatedInt32(301);
    nestedUnknown.addRepeatedInt32(302);

    // Unknown.
    message.setUnknown(1000, 301);
    message.setUnknown(1001, 302);
    message.setUnknown(1002, 'hello world');
    message.setUnknown(1002, nestedUnknown);

    nestedUnknown.setUnknown(2000, 401);

    // Serialize.
    const simplified = new TextFormatSerializer().serialize(message);
    const expected = 'optional_int32: 101\n' +
        'repeated_int32: 201\n' +
        'repeated_int32: 202\n' +
        '1000: 301\n' +
        '1001: 302\n' +
        '1002 {\n' +
        '  repeated_int32: 301\n' +
        '  repeated_int32: 302\n' +
        '  2000: 401\n' +
        '}\n';

    assertEquals(expected, simplified);
  },

  testSerializationOfUnknownParsedFromObject() {
    // Construct the object-serialized representation of the message constructed
    // programmatically in the test above.
    const serialized = {
      1: 101,
      31: [201, 202],
      1000: 301,
      1001: 302,
      1002: {31: [301, 302], 2000: 401},
    };

    // Deserialize that representation into a TestAllTypes message.
    const objectSerializer = new ObjectSerializer();
    const message = new TestAllTypes();
    objectSerializer.deserializeTo(message, serialized);

    // Check that the text format matches what we expect.
    const simplified = new TextFormatSerializer().serialize(message);
    const expected =
        ('optional_int32: 101\n' +
         'repeated_int32: 201\n' +
         'repeated_int32: 202\n' +
         '1000: 301\n' +
         '1001: 302\n' +
         '1002 {\n' +
         '  31: 301\n' +
         '  31: 302\n' +
         '  2000: 401\n' +
         '}\n');
    assertEquals(expected, simplified);
  },

  testTokenizer() {
    const types = TextFormatSerializer.Tokenizer_.TokenTypes;
    assertTokens('{ 123 }', [
      {type: types.OPEN_BRACE},
      {type: types.WHITESPACE, value: ' '},
      {type: types.NUMBER, value: '123'},
      {type: types.WHITESPACE, value: ' '},
      {type: types.CLOSE_BRACE},
    ]);
    // The c++ proto serializer might represent a float in exponential
    // notation:
    assertTokens('{ 1.2345e+3 }', [
      {type: types.OPEN_BRACE},
      {type: types.WHITESPACE, value: ' '},
      {type: types.NUMBER, value: '1.2345e+3'},
      {type: types.WHITESPACE, value: ' '},
      {type: types.CLOSE_BRACE},
    ]);
  },

  testTokenizerExponentialFloatProblem() {
    const input = 'merchant: {              # blah blah\n' +
        '    total_price: 3.2186e+06      # 3_218_600; 3.07Mi\n' +
        '    taxes      : 2.17199e+06\n' +
        '}';
    const types = TextFormatSerializer.Tokenizer_.TokenTypes;
    assertTokens(
        input,
        [
          {type: types.IDENTIFIER, value: 'merchant'},
          {type: types.COLON, value: ':'},
          {type: types.OPEN_BRACE, value: '{'},
          {type: types.COMMENT, value: '# blah blah'},
          {type: types.IDENTIFIER, value: 'total_price'},
          {type: types.COLON, value: ':'},
          {type: types.NUMBER, value: '3.2186e+06'},
          {type: types.COMMENT, value: '# 3_218_600; 3.07Mi'},
          {type: types.IDENTIFIER, value: 'taxes'},
          {type: types.COLON, value: ':'},
          {type: types.NUMBER, value: '2.17199e+06'},
          {type: types.CLOSE_BRACE, value: '}'},
        ],
        true);
  },

  testTokenizerNoWhitespace() {
    const types = TextFormatSerializer.Tokenizer_.TokenTypes;
    assertTokens(
        '{ "hello world" }',
        [
          {type: types.OPEN_BRACE},
          {type: types.STRING, value: '"hello world"'},
          {type: types.CLOSE_BRACE},
        ],
        true);
  },

  testTokenizerSingleTokens() {
    const types = TextFormatSerializer.Tokenizer_.TokenTypes;
    assertTokens('{', {type: types.OPEN_BRACE});
    assertTokens('}', {type: types.CLOSE_BRACE});
    assertTokens('<', {type: types.OPEN_TAG});
    assertTokens('>', {type: types.CLOSE_TAG});
    assertTokens(':', {type: types.COLON});
    assertTokens(',', {type: types.COMMA});
    assertTokens(';', {type: types.SEMI});

    assertIdentifier('abcd');
    assertIdentifier('Abcd');
    assertIdentifier('ABcd');
    assertIdentifier('ABcD');
    assertIdentifier('a123nc');
    assertIdentifier('a45_bC');
    assertIdentifier('A45_bC');

    assertIdentifier('inf');
    assertIdentifier('infinity');
    assertIdentifier('nan');

    assertNumber(0);
    assertNumber(10);
    assertNumber(123);
    assertNumber(1234);
    assertNumber(123.56);
    assertNumber(-124);
    assertNumber(-1234);
    assertNumber(-123.56);
    assertNumber('123f');
    assertNumber('123.6f');
    assertNumber('-123f');
    assertNumber('-123.8f');
    assertNumber('0x1234');
    assertNumber('0x12ac34');
    assertNumber('0x49e281db686fb');
    // Floating point numbers might be serialized in exponential
    // notation:
    assertNumber('1.2345e+3');
    assertNumber('1.2345e3');
    assertNumber('1.2345e-2');

    assertString('""');
    assertString('"hello world"');
    assertString('"hello # world"');
    assertString('"hello #\\" world"');
    assertString('"|"');
    assertString('"\\"\\""');
    assertString('"\\"foo\\""');
    assertString('"\\"foo\\" and \\"bar\\""');
    assertString('"foo \\"and\\" bar"');

    assertComment('# foo bar baz');
    assertComment('# foo ## bar baz');
    assertComment('# foo "bar" baz');
  },

  testSerializationOfStringWithQuotes() {
    const nestedUnknown = new TestAllTypes();
    const message = new TestAllTypes();
    message.setOptionalString('hello "world"');

    // Serialize.
    const simplified = new TextFormatSerializer().serialize(message);
    const expected = 'optional_string: "hello \\"world\\""\n';
    assertEquals(expected, simplified);
  },

  testDeserialization() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 101\n' +
        'repeated_int32: 201\n' +
        'repeated_int32: 202\n' +
        'optional_float: 123.4';

    new TextFormatSerializer().deserializeTo(message, value);

    assertEquals(101, message.getOptionalInt32());
    assertEquals(201, message.getRepeatedInt32(0));
    assertEquals(202, message.getRepeatedInt32(1));
    assertEquals(123.4, message.getOptionalFloat());
  },

  testDeserializationOfList() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 101\n' +
        'repeated_int32: [201, 202]\n' +
        'optional_float: 123.4';

    new TextFormatSerializer().deserializeTo(message, value);

    assertEquals(101, message.getOptionalInt32());
    assertEquals(201, message.getRepeatedInt32(0));
    assertEquals(123.4, message.getOptionalFloat());
  },

  testDeserializationOfIntegerAsHexadecimalString() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 0x1\n' +
        'optional_sint32: 0xf\n' +
        'optional_uint32: 0xffffffff\n' +
        'repeated_int32: [0x0, 0xff]\n';

    new TextFormatSerializer().deserializeTo(message, value);

    assertEquals(1, message.getOptionalInt32());
    assertEquals(15, message.getOptionalSint32());
    assertEquals(4294967295, message.getOptionalUint32());
    assertEquals(0, message.getRepeatedInt32(0));
    assertEquals(255, message.getRepeatedInt32(1));
  },

  testDeserializationOfInt64AsHexadecimalString() {
    const message = new TestAllTypes();
    const value = 'optional_int64: 0xf';

    new TextFormatSerializer().deserializeTo(message, value);

    assertEquals('0xf', message.getOptionalInt64());
  },

  testDeserializationOfZeroFalseAndEmptyString() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 0\n' +
        'optional_bool: false\n' +
        'optional_string: ""';

    new TextFormatSerializer().deserializeTo(message, value);

    assertEquals(0, message.getOptionalInt32());
    assertEquals(false, message.getOptionalBool());
    assertEquals('', message.getOptionalString());
  },

  testDeserializationOfConcatenatedString() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 123\n' +
        'optional_string:\n' +
        '    "FirstLine"\n' +
        '    "SecondLine"\n' +
        'optional_float: 456.7';

    new TextFormatSerializer().deserializeTo(message, value);

    assertEquals(123, message.getOptionalInt32());
    assertEquals('FirstLineSecondLine', message.getOptionalString());
    assertEquals(456.7, message.getOptionalFloat());
  },

  testDeserializationSkipComment() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 101\n' +
        'repeated_int32: 201\n' +
        '# Some comment.\n' +
        'repeated_int32: 202\n' +
        'optional_float: 123.4';

    const parser = new TextFormatSerializer.Parser();
    assertTrue(parser.parse(message, value));

    assertEquals(101, message.getOptionalInt32());
    assertEquals(201, message.getRepeatedInt32(0));
    assertEquals(202, message.getRepeatedInt32(1));
    assertEquals(123.4, message.getOptionalFloat());
  },

  testDeserializationSkipTrailingComment() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 101\n' +
        'repeated_int32: 201\n' +
        'repeated_int32: 202  # Some trailing comment.\n' +
        'optional_float: 123.4';

    const parser = new TextFormatSerializer.Parser();
    assertTrue(parser.parse(message, value));

    assertEquals(101, message.getOptionalInt32());
    assertEquals(201, message.getRepeatedInt32(0));
    assertEquals(202, message.getRepeatedInt32(1));
    assertEquals(123.4, message.getOptionalFloat());
  },

  testDeserializationSkipUnknown() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 101\n' +
        'repeated_int32: 201\n' +
        'some_unknown: true\n' +
        'repeated_int32: 202\n' +
        'optional_float: 123.4';

    const parser = new TextFormatSerializer.Parser();
    assertTrue(parser.parse(message, value, true));

    assertEquals(101, message.getOptionalInt32());
    assertEquals(201, message.getRepeatedInt32(0));
    assertEquals(202, message.getRepeatedInt32(1));
    assertEquals(123.4, message.getOptionalFloat());
  },

  testDeserializationSkipUnknownList() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 101\n' +
        'repeated_int32: 201\n' +
        'some_unknown: [true, 1, 201, "hello"]\n' +
        'repeated_int32: 202\n' +
        'optional_float: 123.4';

    const parser = new TextFormatSerializer.Parser();
    assertTrue(parser.parse(message, value, true));

    assertEquals(101, message.getOptionalInt32());
    assertEquals(201, message.getRepeatedInt32(0));
    assertEquals(202, message.getRepeatedInt32(1));
    assertEquals(123.4, message.getOptionalFloat());
  },

  testDeserializationSkipUnknownNested() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 101\n' +
        'repeated_int32: 201\n' +
        'some_unknown: <\n' +
        '  a: 1\n' +
        '  b: 2\n' +
        '>\n' +
        'repeated_int32: 202\n' +
        'optional_float: 123.4';

    const parser = new TextFormatSerializer.Parser();
    assertTrue(parser.parse(message, value, true));

    assertEquals(101, message.getOptionalInt32());
    assertEquals(201, message.getRepeatedInt32(0));
    assertEquals(202, message.getRepeatedInt32(1));
    assertEquals(123.4, message.getOptionalFloat());
  },

  testDeserializationSkipUnknownNestedInvalid() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 101\n' +
        'repeated_int32: 201\n' +
        'some_unknown: <\n' +
        '  a: \n' +  // Missing value.
        '  b: 2\n' +
        '>\n' +
        'repeated_int32: 202\n' +
        'optional_float: 123.4';

    const parser = new TextFormatSerializer.Parser();
    assertFalse(parser.parse(message, value, true));
  },

  testDeserializationSkipUnknownNestedInvalid2() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 101\n' +
        'repeated_int32: 201\n' +
        'some_unknown: <\n' +
        '  a: 2\n' +
        '  b: 2\n' +
        '}\n' +  // Delimiter mismatch
        'repeated_int32: 202\n' +
        'optional_float: 123.4';

    const parser = new TextFormatSerializer.Parser();
    assertFalse(parser.parse(message, value, true));
  },

  testDeserializationLegacyFormat() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 101,\n' +
        'repeated_int32: 201,\n' +
        'repeated_int32: 202;\n' +
        'optional_float: 123.4';

    new TextFormatSerializer().deserializeTo(message, value);

    assertEquals(101, message.getOptionalInt32());
    assertEquals(201, message.getRepeatedInt32(0));
    assertEquals(202, message.getRepeatedInt32(1));
    assertEquals(123.4, message.getOptionalFloat());
  },

  testDeserializationVariedNumbers() {
    const message = new TestAllTypes();
    const value =
        ('repeated_int32: 23\n' +
         'repeated_int32: -3\n' +
         'repeated_int32: 0xdeadbeef\n' +
         'repeated_float: 123.0\n' +
         'repeated_float: -3.27\n' +
         'repeated_float: -35.5f\n');

    new TextFormatSerializer().deserializeTo(message, value);

    assertEquals(23, message.getRepeatedInt32(0));
    assertEquals(-3, message.getRepeatedInt32(1));
    assertEquals(3735928559, message.getRepeatedInt32(2));
    assertEquals(123.0, message.getRepeatedFloat(0));
    assertEquals(-3.27, message.getRepeatedFloat(1));
    assertEquals(-35.5, message.getRepeatedFloat(2));
  },

  testDeserializationScientificNotation() {
    const message = new TestAllTypes();
    const value = 'repeated_float: 1.1e5\n' +
        'repeated_float: 1.1e-5\n' +
        'repeated_double: 1.1e5\n' +
        'repeated_double: 1.1e-5\n';
    new TextFormatSerializer().deserializeTo(message, value);
    assertEquals(1.1e5, message.getRepeatedFloat(0));
    assertEquals(1.1e-5, message.getRepeatedFloat(1));
    assertEquals(1.1e5, message.getRepeatedDouble(0));
    assertEquals(1.1e-5, message.getRepeatedDouble(1));
  },

  testParseNumericalConstant() {
    /** @suppress {visibility} suppression added to enable type checking */
    const parseNumericalConstant =
        TextFormatSerializer.Parser.parseNumericalConstant_;

    assertEquals(Infinity, parseNumericalConstant('inf'));
    assertEquals(Infinity, parseNumericalConstant('inff'));
    assertEquals(Infinity, parseNumericalConstant('infinity'));
    assertEquals(Infinity, parseNumericalConstant('infinityf'));
    assertEquals(Infinity, parseNumericalConstant('Infinityf'));

    assertEquals(-Infinity, parseNumericalConstant('-inf'));
    assertEquals(-Infinity, parseNumericalConstant('-inff'));
    assertEquals(-Infinity, parseNumericalConstant('-infinity'));
    assertEquals(-Infinity, parseNumericalConstant('-infinityf'));
    assertEquals(-Infinity, parseNumericalConstant('-Infinity'));

    assertNull(parseNumericalConstant('-infin'));
    assertNull(parseNumericalConstant('infin'));
    assertNull(parseNumericalConstant('-infinite'));

    assertNull(parseNumericalConstant('-infin'));
    assertNull(parseNumericalConstant('infin'));
    assertNull(parseNumericalConstant('-infinite'));

    assertTrue(isNaN(parseNumericalConstant('Nan')));
    assertTrue(isNaN(parseNumericalConstant('NaN')));
    assertTrue(isNaN(parseNumericalConstant('NAN')));
    assertTrue(isNaN(parseNumericalConstant('nan')));
    assertTrue(isNaN(parseNumericalConstant('nanf')));
    assertTrue(isNaN(parseNumericalConstant('NaNf')));

    assertEquals(Number.POSITIVE_INFINITY, parseNumericalConstant('infinity'));
    assertEquals(Number.NEGATIVE_INFINITY, parseNumericalConstant('-inf'));
    assertEquals(Number.NEGATIVE_INFINITY, parseNumericalConstant('-infinity'));

    assertNull(parseNumericalConstant('na'));
    assertNull(parseNumericalConstant('-nan'));
    assertNull(parseNumericalConstant('none'));
  },

  testDeserializationOfNumericalConstants() {
    const message = new TestAllTypes();
    const value =
        ('repeated_float: inf\n' +
         'repeated_float: -inf\n' +
         'repeated_float: nan\n' +
         'repeated_float: 300.2\n');

    new TextFormatSerializer().deserializeTo(message, value);

    assertEquals(Infinity, message.getRepeatedFloat(0));
    assertEquals(-Infinity, message.getRepeatedFloat(1));
    assertTrue(isNaN(message.getRepeatedFloat(2)));
    assertEquals(300.2, message.getRepeatedFloat(3));
  },

  testGetNumberFromStringExponentialNotation() {
    for (let i = 0; i < floatFormatCases.length; ++i) {
      const thistest = floatFormatCases[i];
      /** @suppress {visibility} suppression added to enable type checking */
      const result =
          TextFormatSerializer.Parser.getNumberFromString_(thistest.given);
      assertEquals(thistest.expect, result);
    }
  },

  testDeserializationExponentialFloat() {
    const parser = new TextFormatSerializer.Parser();
    for (let i = 0; i < floatFormatCases.length; ++i) {
      const thistest = floatFormatCases[i];
      const message = new TestAllTypes();
      const value = 'optional_float: ' + thistest.given;
      assertTrue(parser.parse(message, value, true));
      assertEquals(thistest.expect, message.getOptionalFloat());
    }
  },

  testGetNumberFromString() {
    /** @suppress {visibility} suppression added to enable type checking */
    const getNumberFromString =
        TextFormatSerializer.Parser.getNumberFromString_;

    assertEquals(3735928559, getNumberFromString('0xdeadbeef'));
    assertEquals(4276215469, getNumberFromString('0xFEE1DEAD'));
    assertEquals(123.1, getNumberFromString('123.1'));
    assertEquals(123.0, getNumberFromString('123.0'));
    assertEquals(-29.3, getNumberFromString('-29.3f'));
    assertEquals(23, getNumberFromString('23'));
    assertEquals(-3, getNumberFromString('-3'));
    assertEquals(-3.27, getNumberFromString('-3.27'));

    assertThrows(goog.partial(getNumberFromString, 'cat'));
    assertThrows(goog.partial(getNumberFromString, 'NaN'));
    assertThrows(goog.partial(getNumberFromString, 'inf'));
  },

  testDeserializationError() {
    const message = new TestAllTypes();
    const value = 'optional_int33: 101\n';
    const result = new TextFormatSerializer().deserializeTo(message, value);
    assertEquals(result, 'Unknown field: optional_int33');
  },

  testNestedDeserialization() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 101\n' +
        'optional_nested_message: {\n' +
        '  b: 301\n' +
        '}';

    new TextFormatSerializer().deserializeTo(message, value);

    assertEquals(101, message.getOptionalInt32());
    assertEquals(301, message.getOptionalNestedMessage().getB());
  },

  testNestedDeserializationLegacyFormat() {
    const message = new TestAllTypes();
    const value = 'optional_int32: 101\n' +
        'optional_nested_message: <\n' +
        '  b: 301\n' +
        '>';

    new TextFormatSerializer().deserializeTo(message, value);

    assertEquals(101, message.getOptionalInt32());
    assertEquals(301, message.getOptionalNestedMessage().getB());
  },

  testBidirectional() {
    const message = new TestAllTypes();

    // Set the fields.
    // Singular.
    message.setOptionalInt32(101);
    message.setOptionalInt64('102');
    message.setOptionalUint32(103);
    message.setOptionalUint64('104');
    message.setOptionalSint32(105);
    message.setOptionalSint64('106');
    message.setOptionalFixed32(107);
    message.setOptionalFixed64('108');
    message.setOptionalSfixed32(109);
    message.setOptionalSfixed64('110');
    message.setOptionalFloat(111.5);
    message.setOptionalDouble(112.5);
    message.setOptionalBool(true);
    message.setOptionalString('test');
    message.setOptionalBytes('abcd');

    const group = new TestAllTypes.OptionalGroup();
    group.setA(111);

    message.setOptionalgroup(group);

    const nestedMessage = new TestAllTypes.NestedMessage();
    nestedMessage.setB(112);

    message.setOptionalNestedMessage(nestedMessage);

    message.setOptionalNestedEnum(TestAllTypes.NestedEnum.FOO);

    // Repeated.
    message.addRepeatedInt32(201);
    message.addRepeatedInt32(202);
    message.addRepeatedString('hello "world"');

    // Serialize the message to text form.
    const serializer = new TextFormatSerializer();
    const textform = serializer.serialize(message);

    // Create a copy and deserialize into the copy.
    const copy = new TestAllTypes();
    serializer.deserializeTo(copy, textform);

    // Assert that the messages are structurally equivalent.
    assertTrue(copy.equals(message));
  },

  testBidirectional64BitNumber() {
    const message = new TestAllTypes();
    message.setOptionalInt64Number(10000000);
    message.setOptionalInt64String('200000000000000000');

    // Serialize the message to text form.
    const serializer = new TextFormatSerializer();
    const textform = serializer.serialize(message);

    // Create a copy and deserialize into the copy.
    const copy = new TestAllTypes();
    serializer.deserializeTo(copy, textform);

    // Assert that the messages are structurally equivalent.
    assertTrue(copy.equals(message));
  },

  testUseEnumValues() {
    const message = new TestAllTypes();
    message.setOptionalNestedEnum(TestAllTypes.NestedEnum.FOO);

    const serializer = new TextFormatSerializer(false, true);
    const textform = serializer.serialize(message);

    const expected = 'optional_nested_enum: 0\n';

    assertEquals(expected, textform);

    const deserializedMessage = new TestAllTypes();
    serializer.deserializeTo(deserializedMessage, textform);

    assertEquals(
        TestAllTypes.NestedEnum.FOO,
        deserializedMessage.getOptionalNestedEnum());
  },
});
