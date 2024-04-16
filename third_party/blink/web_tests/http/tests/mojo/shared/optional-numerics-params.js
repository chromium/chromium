// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This file tests that JS can correctly encode optional numerics (by binding
// to an interface implemented in C++) and that JS can correctly decode optional
// numerics params (by binding to a interface implemented in JS).
//
// These tests can be imported as a module or added as a script to tests lite
// bindings.

class ParamsJsImpl {
  constructor() {
    const sendNullMethods = [
      'sendNullBool',
      'sendNullUint8',
      'sendNullInt8',
      'sendNullUint16',
      'sendNullInt16',
      'sendNullUint32',
      'sendNullInt32',
      'sendNullUint64',
      'sendNullInt64',
      'sendNullFloat',
      'sendNullDouble',
      'sendNullEnum',

      'sendNullBools',
      'sendNullInt16s',
      'sendNullUint32s',
      'sendNullDoubles',
      'sendNullEnums',

      'sendNullBoolMap',
      'sendNullDoubleMap',
      'sendNullEnumMap',
    ];
    for (const method of sendNullMethods) {
      this[method] = this.sendNull;
    }

    const sendOptionalMethods = [
      'sendOptionalBool',
      'sendOptionalUint8',
      'sendOptionalInt8',
      'sendOptionalUint16',
      'sendOptionalInt16',
      'sendOptionalUint32',
      'sendOptionalInt32',
      'sendOptionalUint64',
      'sendOptionalInt64',
      'sendOptionalFloat',
      'sendOptionalDouble',
      'sendOptionalEnum',

      'sendOptionalBools',
      'sendOptionalInt16s',
      'sendOptionalUint32s',
      'sendOptionalDoubles',
      'sendOptionalEnums',

      'sendOptionalBoolMap',
      'sendOptionalDoubleMap',
      'sendOptionalEnumMap',
    ];
    for (const method of sendOptionalMethods) {
      this[method] = this.sendOptional;
    }

    this.receiver = new OptionalNumericsParamsReceiver(this);
  }

  async sendNull(value) {
    assert_equals(value, null);
  }

  async sendOptional(value) {
    return {value: value};
  }

  async sendNullStructWithOptionalNumerics(s) {
    assert_equals(s, null);
  }

  async sendStructWithNullOptionalNumerics(s) {
    assert_equals(s.optionalBool, null);
    assert_equals(s.optionalUint8, null);
    assert_equals(s.optionalInt8, null);
    assert_equals(s.optionalUint16, null);
    assert_equals(s.optionalInt16, null);
    assert_equals(s.optionalUint32, null);
    assert_equals(s.optionalInt32, null);
    assert_equals(s.optionalUint64, null);
    assert_equals(s.optionalInt64, null);
    assert_equals(s.optionalFloat, null);
    assert_equals(s.optionalDouble, null);
    assert_equals(s.optionalEnum, null);
  }

  async sendStructWithOptionalNumerics(s) {
    return {
      optionalBool: s.optionalBool,
      optionalUint8: s.optionalUint8,
      optionalInt8: s.optionalInt8,
      optionalUint16: s.optionalUint16,
      optionalInt16: s.optionalInt16,
      optionalUint32: s.optionalUint32,
      optionalInt32: s.optionalInt32,
      optionalUint64: s.optionalUint64,
      optionalInt64: s.optionalInt64,
      optionalFloat: s.optionalFloat,
      optionalDouble: s.optionalDouble,
      optionalEnum: s.optionalEnum,
    };
  }
}

const cpp = new OptionalNumericsParamsRemote();
cpp.$.bindNewPipeAndPassReceiver().bindInBrowser('process');

const jsImpl = new ParamsJsImpl();
const js = jsImpl.receiver.$.bindNewPipeAndPassRemote();

function assert_empty_response(response) {
  assert_equals(Object.keys(response).length, 0);
};

const testNullMethods = [{
  method: 'sendNullBool',
  numericalType: 'bool'
}, {
  method: 'sendNullUint8',
  numericalType: 'uint8'
}, {
  method: 'sendNullInt8',
  numericalType: 'int8'
}, {
  method: 'sendNullUint16',
  numericalType: 'uint16'
}, {
  method: 'sendNullInt16',
  numericalType: 'int16'
}, {
  method: 'sendNullUint32',
  numericalType: 'uint32'
}, {
  method: 'sendNullInt32',
  numericalType: 'int32'
}, {
  method: 'sendNullUint64',
  numericalType: 'uint64'
}, {
  method: 'sendNullInt64',
  numericalType: 'int64'
}, {
  method: 'sendNullFloat',
  numericalType: 'float'
}, {
  method: 'sendNullDouble',
  numericalType: 'double'
}, {
  method: 'sendNullEnum',
  numericalType: 'enum'
}];

for (const {method, numericalType} of testNullMethods) {
  promise_test(async () => {
    assert_empty_response(await cpp[method]());
    assert_empty_response(await cpp[method](null));
    assert_empty_response(await cpp[method](undefined));
  }, `JS encoding and C++ decoding of null ${numericalType}.`);

  promise_test(async () => {
    assert_empty_response(await js[method]());
    assert_empty_response(await js[method](null));
    assert_empty_response(await js[method](undefined));
  }, `JS decoding of null ${numericalType} param.`);
}

promise_test(async () => {
  assert_empty_response(await cpp.sendNullBools([null, null]));
  assert_empty_response(await cpp.sendNullInt16s([null, null]));
  assert_empty_response(await cpp.sendNullUint32s([null, null]));
  assert_empty_response(await cpp.sendNullDoubles([null, null]));
  assert_empty_response(await cpp.sendNullEnums([null, null]));
});

promise_test(async () => {
  assert_empty_response(await cpp.sendNullBoolMap({0: null}));
  assert_empty_response(await cpp.sendNullDoubleMap({1: null}));
  assert_empty_response(await cpp.sendNullEnumMap({2: null}));
});

promise_test(async () => {
  assert_empty_response(await cpp.sendNullStructWithOptionalNumerics(null));
}, `JS encoding and C++ decoding of null struct with optional numerics.`);

promise_test(async () => {
  assert_empty_response(await js.sendNullStructWithOptionalNumerics(null));
}, `JS decoding of null struct with optional numerics.`);

function assert_value_equals(response, expectedValue) {
  assert_equals(response.value, expectedValue);
}

const testMethods = [{
  method: 'sendOptionalBool',
  valueToUse: true,
  numericalType: 'bool',
}, {
  method: 'sendOptionalUint8',
  valueToUse: 8,
  numericalType: 'uint8',
}, {
  method: 'sendOptionalInt8',
  valueToUse: -8,
  numericalType: 'int8',
}, {
  method: 'sendOptionalUint16',
  valueToUse: 16,
  numericalType: 'uint16',
}, {
  method: 'sendOptionalInt16',
  valueToUse: -16,
  numericalType: 'int16',
}, {
  method: 'sendOptionalUint32',
  valueToUse: 32,
  numericalType: 'uint32',
}, {
  method: 'sendOptionalInt32',
  valueToUse: -32,
  numericalType: 'int32',
}, {
  method: 'sendOptionalUint64',
  valueToUse: BigInt("64"),
  numericalType: 'uint64',
}, {
  method: 'sendOptionalInt64',
  valueToUse: BigInt("-64"),
  numericalType: 'int64',
}, {
  method: 'sendOptionalFloat',
  valueToUse: -0.5,
  numericalType: 'float',
}, {
  method: 'sendOptionalDouble',
  valueToUse: 0.25,
  numericalType: 'double',
}, {
  method: 'sendOptionalEnum',
  valueToUse: OptionalNumericsRegularEnum.kBar,
  numericalType: 'enum',
}];

for (const {method, valueToUse, numericalType} of testMethods) {
  promise_test(async () => {
    assert_value_equals(await cpp[method](valueToUse), valueToUse);
  }, `JS encoding and C++ decoding of optional ${numericalType}.`);
}

const testArrayMethods = [{
  method: 'sendOptionalBools',
  valuesToUse: [true, null, false, true],
  expected: [true, false, true],
}, {
  method: 'sendOptionalInt16s',
  valuesToUse: [3, null, 2, 1],
  expected: [3, 2, 1],
}, {
  method: 'sendOptionalUint32s',
  valuesToUse: [null, 1],
  expected: [1],
}, {
  method: 'sendOptionalDoubles',
  valuesToUse: [6.66, 9.99],
  expected: [6.66, 9.99],
}, {
  method: 'sendOptionalEnums',
  valuesToUse: [null, OptionalNumericsRegularEnum.kBar, null, null],
  expected: [OptionalNumericsRegularEnum.kBar],
}];

for (const {method, valuesToUse, expected} of testArrayMethods) {
  promise_test(async() => {
    const response = await cpp[method](valuesToUse);
    assert_array_equals(response.values, expected, `JS encoding and C++ decoding of: ${method}`);
  });
}

const testMapMethods = [{
  method: 'sendOptionalBoolMap',
  valuesToUse: {0: true, 1: null, 2:false},
  expected: {0: true, 2: false},
}, {
  method: 'sendOptionalDoubleMap',
  valuesToUse: {3: 3.33, 4: null},
  expected: {3: 3.33},
}, {
  method: 'sendOptionalEnumMap',
  valuesToUse: {5: null, 6: OptionalNumericsRegularEnum.kBar},
  expected: {6: OptionalNumericsRegularEnum.kBar},
}];

for (const {method, valuesToUse, expected} of testMapMethods) {
  promise_test(async() => {
    const response = await cpp[method](valuesToUse);
    assert_object_equals(response.values, expected, `JS encoding and C++ decoding of: ${method}`);
  });
}

const structFields = [
  'optionalBool',
  'optionalUint8',
  'optionalInt8',
  'optionalUint16',
  'optionalInt16',
  'optionalUint32',
  'optionalInt32',
  'optionalUint64',
  'optionalInt64',
  'optionalFloat',
  'optionalDouble',
  'optionalEnum',
];

promise_test(async () => {
  assert_empty_response(await cpp.sendStructWithNullOptionalNumerics({}));

  const structWithNullFields = {};
  const structWithUndefinedFields = {};
  for (const field of structFields) {
    structWithNullFields[field] = null;
    structWithUndefinedFields[field] = undefined;
  }

  assert_empty_response(await cpp.sendStructWithNullOptionalNumerics(
    structWithNullFields));
  assert_empty_response(await cpp.sendStructWithNullOptionalNumerics(
    structWithUndefinedFields));

}, 'JS encoding and C++ decoding of struct with null optional numerics.');

promise_test(async () => {
  assert_empty_response(await cpp.sendStructWithNullOptionalNumerics({}));

  const structWithNullFields = {};
  const structWithUndefinedFields = {};
  for (const field of structFields) {
    structWithNullFields[field] = null;
    structWithUndefinedFields[field] = undefined;
  }

  assert_empty_response(await js.sendStructWithNullOptionalNumerics(
    structWithNullFields));
  assert_empty_response(await js.sendStructWithNullOptionalNumerics(
    structWithUndefinedFields));

}, 'JS decoding of struct param with null optional numerics.');

const testStructFields = [{
    name: 'optionalBool',
    value: true,
    responseArgName: 'boolValue'
  }, {
    name: 'optionalUint8',
    value: 8,
    responseArgName: 'uint8Value'
  }, {
    name: 'optionalInt8',
    value: -8,
    responseArgName: 'int8Value'
  }, {
    name: 'optionalUint16',
    value: 16,
    responseArgName: 'uint16Value'
  }, {
    name: 'optionalInt16',
    value: -16,
    responseArgName: 'int16Value'
  }, {
    name: 'optionalUint32',
    value: 32,
    responseArgName: 'uint32Value'
  }, {
    name: 'optionalInt32',
    value: -32,
    responseArgName: 'int32Value'
  }, {
    name: 'optionalUint64',
    value: BigInt("64"),
    responseArgName: 'uint64Value'
  }, {
    name: 'optionalInt64',
    value: BigInt("-64"),
    responseArgName: 'int64Value'
  }, {
    name: 'optionalFloat',
    value: -0.5,
    responseArgName: 'floatValue'
  }, {
    name: 'optionalDouble',
    value: 0.25,
    responseArgName: 'doubleValue'
  }, {
    name: 'optionalEnum',
    value: OptionalNumericsRegularEnum.kFoo,
    responseArgName: 'enumValue'
}];

promise_test(async () => {
  const testStruct = {};
  for (const {name, value} of testStructFields) {
    testStruct[name] = value;
  }

  const response = await cpp.sendStructWithOptionalNumerics(testStruct);

  for (const {name, value, responseArgName} of testStructFields) {
    assert_equals(response[responseArgName], value, `${name}:`);
  }
}, 'JS encoding and C++ decoding of struct with optional numerics.');

promise_test(async () => {
  const testStruct = {};
  for (const {name, value} of testStructFields) {
    testStruct[name] = value;
  }

  const response = await cpp.sendStructWithOptionalNumerics(testStruct);

  for (const {name, value, responseArgName} of testStructFields) {
    assert_equals(response[responseArgName], value, `${name}:`);
  }
}, 'JS decoding of struct param with optional numerics.');
