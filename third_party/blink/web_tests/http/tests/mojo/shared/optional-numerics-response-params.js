// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This file tests that JS can correctly decode optional numerics response
// params  (by binding to an interface implemented in C++) and that JS can
// correctly encode optional numerics response params (by binding to a interface
// implemented in JS).
//
// These tests can be imported as a module or added as a script to tests lite
// bindings.

class ResponseParamsJsImpl {
  constructor() {
    const nullMethods = [
      'getNullBool',
      'getNullUint8',
      'getNullInt8',
      'getNullUint16',
      'getNullInt16',
      'getNullUint32',
      'getNullInt32',
      'getNullUint64',
      'getNullInt64',
      'getNullFloat',
      'getNullDouble',
      'getNullEnum',

      'getNullBools',
      'getNullInt16s',
      'getNullUint32s',
      'getNullDoubles',
      'getNullEnums',

      'getNullBoolMap',
      'getNullInt32Map',
      'getNullEnumMap',
    ];
    for (const method of nullMethods) {
      this[method] = this.getNullOptional;
    }

    const methods = [
      'getOptionalBool',
      'getOptionalUint8',
      'getOptionalInt8',
      'getOptionalUint16',
      'getOptionalInt16',
      'getOptionalUint32',
      'getOptionalInt32',
      'getOptionalUint64',
      'getOptionalInt64',
      'getOptionalFloat',
      'getOptionalDouble',
      'getOptionalEnum',

      'getOptionalBools',
      'getOptionalInt16s',
      'getOptionalUint32s',
      'getOptionalDoubles',
      'getOptionalEnums',

      'getOptionalBoolMap',
      'getOptionalFloatMap',
      'getOptionalEnumMap',
    ];
    for (const method of methods) {
      this[method] = this.getOptional;
    }

    this.receiver = new OptionalNumericsResponseParamsReceiver(this);
  }

  async getOptional(value) {
    return {optionalValue: value};
  }

  async getNullOptional() {
    if (typeof this.nullType_ === 'undefined') {
      throw new Error('setNullForNextResponse should be called first.');
    }

    const response = {};
    switch (this.nullType_) {
      case 'undefined':
        response.optionalValue = undefined;
        break;
      case 'null':
        response.optionalValue = null;
        break;
      case 'empty':
        break;
    }

    delete this.nullType_;
    return response;
  }

  // nullType can be `undefined`, `null`, or `empty` and they correspond to
  // what we use as a response for null values.
  setNullForNextResponse(nullType) {
    assert_true(nullType === 'undefined' ||
                nullType === 'null' ||
                nullType === 'empty');
    this.nullType_ = nullType;
  }

  async getNullStructWithOptionalNumerics() {
    let response = {};
    switch (this.nullType_) {
      case 'undefined':
        response.s = undefined;
        break;
      case 'null':
        response.s = null;
        break;
      case 'empty':
        break;
    }

    delete this.nullType_;
    return response;
  }

  async getStructWithNullOptionalNumerics() {
    if (typeof this.nullType_ === 'undefined') {
      throw new Error('setNullForNextResponse should be called first.');
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

    let s = {};
    switch (this.nullType_) {
      case 'undefined':
        for (const field of structFields) {
          s[field] = undefined;
        }
        break;
      case 'null':
        for (const field of structFields) {
          s[field] = null;
        }
        break;
      case 'empty':
        break;
    }

    delete this.nullType_;
    return {s};
  }

  async getStructWithOptionalNumerics(
    boolValue,
    uint8Value,
    int8Value,
    uint16Value,
    int16Value,
    uint32Value,
    int32Value,
    uint64Value,
    int64Value,
    floatValue,
    doubleValue,
    enumValue) {
    const struct = {
      optionalBool: boolValue,
      optionalUint8: uint8Value,
      optionalInt8: int8Value,
      optionalUint16: uint16Value,
      optionalInt16: int16Value,
      optionalUint32: uint32Value,
      optionalInt32: int32Value,
      optionalUint64: uint64Value,
      optionalInt64: int64Value,
      optionalFloat: floatValue,
      optionalDouble: doubleValue,
      optionalEnum: enumValue,
    };

    return {
      s: struct
    };
  }
};

const cpp = new OptionalNumericsResponseParamsRemote();
cpp.$.bindNewPipeAndPassReceiver().bindInBrowser('process');

const jsImpl = new ResponseParamsJsImpl();
const js = jsImpl.receiver.$.bindNewPipeAndPassRemote();

const testGetNullMethods = [{
  method: 'getNullBool',
  numericType: 'bool',
}, {
  method: 'getNullUint8',
  numericType: 'uint8',
}, {
  method: 'getNullInt8',
  numericType: 'int8',
}, {
  method: 'getNullUint16',
  numericType: 'uint16',
}, {
  method: 'getNullInt16',
  numericType: 'int16',
}, {
  method: 'getNullUint32',
  numericType: 'uint32',
}, {
  method: 'getNullInt32',
  numericType: 'int32',
}, {
  method: 'getNullUint64',
  numericType: 'uint64',
}, {
  method: 'getNullInt64',
  numericType: 'int64',
}, {
  method: 'getNullFloat',
  numericType: 'float',
}, {
  method: 'getNullDouble',
  numericType: 'double',
}, {
  method: 'getNullEnum',
  numericType: 'enum',
}];

for (const {method, responseArgName, numericType} of testGetNullMethods) {
  promise_test(async () => {
    const response = await cpp[method]();
    assert_true('optionalValue' in response);
    assert_equals(response.optionalValue, null);
  }, `C++ encoding and JS decoding of null ${numericType} response param.`);

  promise_test(async () => {
    {
      jsImpl.setNullForNextResponse('null');
      const response = await js[method]();
      assert_true('optionalValue' in response);
      assert_equals(response.optionalValue, null);
    }
    {
      jsImpl.setNullForNextResponse('undefined');
      const response = await js[method]();
      assert_true('optionalValue' in response);
      assert_equals(response.optionalValue, null);
    }
    {
      jsImpl.setNullForNextResponse('empty');
      const response = await js[method]();
      assert_true('optionalValue' in response);
      assert_equals(response.optionalValue, null);
    }
  }, `JS decoding of null ${numericType} response param.`);
}

promise_test(async () => {
  const response = await cpp.getNullStructWithOptionalNumerics();
  assert_true('s' in response);
  assert_equals(response.s, null);
}, 'C++ encoding and JS decoding of null struct with optional numerics ' +
   'in response param.');

promise_test(async () => {
    {
      jsImpl.setNullForNextResponse('null');
      const response = await js.getNullStructWithOptionalNumerics();
      assert_true('s' in response);
      assert_equals(response.s, null);
    }
    {
      jsImpl.setNullForNextResponse('undefined');
      const response = await js.getNullStructWithOptionalNumerics();
      assert_true('s' in response);
      assert_equals(response.s, null);
    }
    {
      jsImpl.setNullForNextResponse('empty');
      const response = await js.getNullStructWithOptionalNumerics();
      assert_true('s' in response);
      assert_equals(response.s, null);
    }
}, 'JS decoding of null struct with optional numerics.');


const testGetArraysOfNullsMethods = [
  'getNullBools',
  'getNullInt16s',
  'getNullUint32s',
  'getNullDoubles',
  'getNullEnums',
];

for (const method of testGetArraysOfNullsMethods) {
  promise_test(async() => {
    const response = await cpp[method]();
    assert_array_equals(response.optionalValues, [null]);
  });
}

const testGetMapOfNullsMethods = [
  'getNullBoolMap',
  'getNullInt32Map',
  'getNullEnumMap',
];

for (const method of testGetMapOfNullsMethods) {
  promise_test(async() => {
    const response = await cpp[method]();
    assert_object_equals(response.optionalValues, {0: null});
  });
}

const testMethods = [{
  method: 'getOptionalBool',
  valueToUse: true,
  numericType: 'bool',
}, {
  method: 'getOptionalUint8',
  valueToUse: 8,
  numericType: 'uint8',
}, {
  method: 'getOptionalInt8',
  valueToUse: -8,
  numericType: 'int8',
}, {
  method: 'getOptionalUint16',
  valueToUse: 16,
  numericType: 'uint16',
}, {
  method: 'getOptionalInt16',
  valueToUse: -16,
  numericType: 'int16',
}, {
  method: 'getOptionalUint32',
  valueToUse: 32,
  numericType: 'uint32',
}, {
  method: 'getOptionalInt32',
  valueToUse: -32,
  numericType: 'int32',
}, {
  method: 'getOptionalUint64',
  valueToUse: BigInt('64'),
  numericType: 'uint64',
}, {
  method: 'getOptionalInt64',
  valueToUse: BigInt('-64'),
  numericType: 'int64',
}, {
  method: 'getOptionalFloat',
  valueToUse: -0.5,
  numericType: 'float',
}, {
  method: 'getOptionalDouble',
  valueToUse: 0.25,
  numericType: 'double',
}, {
  method: 'getOptionalEnum',
  valueToUse: OptionalNumericsRegularEnum.kBar,
  numericType: 'enum',
}];

for (const {method, valueToUse, numericType} of testMethods) {
  promise_test(async () => {
    const {optionalValue} = await cpp[method](valueToUse);
    assert_equals(optionalValue, valueToUse);
  }, `C++ encoding and JS decoding of optional ${numericType} response param.`);

  promise_test(async () => {
    const {optionalValue} = await js[method](valueToUse);
    assert_equals(optionalValue, valueToUse);
  }, `JS encoding and decoding of ${numericType} response param.`);
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
  const {s} = await cpp.getStructWithNullOptionalNumerics();
  for (const field of structFields) {
    assert_true(field in s);
    assert_equals(s[field], null);
  }
}, 'C++ encoding and JS decoding of struct with null optional numerics ' +
   'in response params.');

promise_test(async () => {
  {
    jsImpl.setNullForNextResponse('empty');
    const {s} = await js.getStructWithNullOptionalNumerics();
    for (const field of structFields) {
      assert_true(field in s);
      assert_equals(s[field], null);
    }
  }
  {
    jsImpl.setNullForNextResponse('undefined');
    const {s} = await js.getStructWithNullOptionalNumerics();
    for (const field of structFields) {
      assert_true(field in s);
      assert_equals(s[field], null);
    }
  }
  {
    jsImpl.setNullForNextResponse('null');
    const {s} = await js.getStructWithNullOptionalNumerics();
    for (const field of structFields) {
      assert_true(field in s);
      assert_equals(s[field], null);
    }
  }
}, 'JS encoding and decoding of struct with null optional numerics in ' +
   'response params.');

const testStructFields = [{
    name: 'optionalBool',
    value: true,
  }, {
    name: 'optionalUint8',
    value: 8,
  }, {
    name: 'optionalInt8',
    value: -8,
  }, {
    name: 'optionalUint16',
    value: 16,
  }, {
    name: 'optionalInt16',
    value: -16,
  }, {
    name: 'optionalUint32',
    value: 32,
  }, {
    name: 'optionalInt32',
    value: -32,
  }, {
    name: 'optionalUint64',
    value: BigInt("64"),
  }, {
    name: 'optionalInt64',
    value: BigInt("-64"),
  }, {
    name: 'optionalFloat',
    value: -0.5,
  }, {
    name: 'optionalDouble',
    value: 0.25,
  }, {
    name: 'optionalEnum',
    value: OptionalNumericsRegularEnum.kFoo,
}];

promise_test(async () => {
  const args = testStructFields.map((field) => {
    return field.value;
  });

  const {s} = await cpp.getStructWithOptionalNumerics(...args);
  assert_equals(Object.keys(s).length, testStructFields.length);

  for (const field of testStructFields) {
    assert_true(field.name in s, field.name);
    assert_equals(s[field.name], field.value, field.name);
  }
}, 'C++ encoding and JS decoding of struct with optional numerics in ' +
   'response params.');

promise_test(async () => {
  const args = testStructFields.map((field) => {
    return field.value;
  });

  const {s} = await js.getStructWithOptionalNumerics(...args);
  assert_equals(Object.keys(s).length, testStructFields.length);

  for (const field of testStructFields) {
    assert_true(field.name in s, field.name);
    assert_equals(s[field.name], field.value, field.name);
  }
}, 'JS encoding of struct with optional numerics in response params.');

const testNullWrapping = [{
  name: 'getOptionalBools',
  value: true,
}, {
  name: 'getOptionalInt16s',
  value: 16,
}, {
  name: 'getOptionalUint32s',
  value: 32,
}, {
  name: 'getOptionalDoubles',
  value: 22.2,
}, {
  name: 'getOptionalEnums',
  value: OptionalNumericsRegularEnum.kFoo,
}];

for (const {name, value} of testNullWrapping) {
  promise_test(async() => {
    const response = await cpp[name](value);
    assert_array_equals(response.optionalValues, [null, value, null]);
  });
}

const testNullWrappingForMap = [{
  name: 'getOptionalBoolMap',
  key: 6,
  value: false,
}, {
  name: 'getOptionalFloatMap',
  key: 7,
  value: 1.25,
}, {
  name: 'getOptionalEnumMap',
  key: 8,
  value: OptionalNumericsRegularEnum.kFoo,
}];

for (const {name, key, value} of testNullWrappingForMap) {
  promise_test(async() => {
    const response = await cpp[name](key, value);
    assert_object_equals(response.optionalValues, {
      [key - 1]: null,
      [key]: value,
      [key + 1]: null,
    });
  });
}
