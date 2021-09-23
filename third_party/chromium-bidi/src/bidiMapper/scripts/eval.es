/**
 * Copyright 2021 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

(() => {
  class Serializer {
    _idToObject = new Map();
    _objectToId = new WeakMap();

    constructor() {}

    serialize(value) {
      if (value === undefined) {
        return { type: 'undefined' };
      } else if (value === null) {
        return { type: 'null' };
      } else if (typeof value === 'string') {
        return { type: 'string', value };
      } else if (typeof value === 'number') {
        let serialized;
        if (Number.isNaN(value)) {
          serialized = 'NaN';
        } else if (Object.is(value, -0)) {
          serialized = '-0';
        } else if (value === -Infinity) {
          serialized = '-Infinity';
        } else if (value === +Infinity) {
          serialized = '+Infinity';
        } else {
          serialized = value;
        }
        return { type: 'number', value: serialized };
      } else if (typeof value === 'boolean') {
        return { type: 'boolean', value };
      } else if (value instanceof Object) {
        // TODO: Recursive serialize.
        let objectId = this._objectToId.get(value);
        if (!objectId) {
          objectId = uuid();
          this._objectToId.set(value, objectId);
          this._idToObject.set(objectId, new WeakRef(value));
        }
        return { type: 'object', objectId };
      } else {
        throw new Error('not yet implemented');
      }
    }

    deserialize(value) {
      switch (value.type) {
        case 'undefined': {
          return undefined;
        }
        case 'null': {
          return null;
        }
        case 'string': {
          return value.value;
        }
        case 'number': {
          if (value.value === 'NaN') {
            return NaN;
          } else if (value.value === '-0') {
            return -0;
          } else if (value.value === '+Infinity') {
            return +Infinity;
          } else if (value.value === '-Infinity') {
            return -Infinity;
          } else {
            return value.value;
          }
        }
        case 'boolean': {
          return value.value;
        }
        case 'object': {
          const weakRef = this._idToObject.get(value.objectId);
          if (!weakRef) {
            throw new Error('unknown object reference');
          }

          const obj = weakRef.deref();
          if (!obj) {
            throw new Error('stable object reference');
          }

          return obj;
        }
        default:
          throw new Error('not yet implemented');
      }
    }
  }

  function uuid() {
    return crypto.randomUUID();
  }

  return function evaluate(script, args) {
    if (window.__webdriver_js_serializer === undefined) {
      window.__webdriver_js_serializer = new Serializer();
    }

    const serializer = window.__webdriver_js_serializer;
    try {
      const deserializedArgs = args.map((arg) => serializer.deserialize(arg));
      const func = new Function(`return (${script})`);
      const result = func.apply(null, deserializedArgs);
      const serializedResult = serializer.serialize(result);
      return { result: serializedResult };
    } catch (e) {
      if (e instanceof Error) {
        return { exceptionDetails: { message: e.message, stacktrace: e.stack } };
      } else {
        return { exceptionDetails: { value: serializer.serialize(e) } };
      }
    }
  };
})()
