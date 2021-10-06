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
  class ObjectCache {
    _idToObject = new Map();
    _objectToId = new WeakMap();
  }

  function getObjectCache() {
    if (globalThis.__webdriver_objectCache === undefined) {
      globalThis.__webdriver_objectCache = new ObjectCache();
    }

    return globalThis.__webdriver_objectCache;
  }

  const objectCache = getObjectCache();

  function serialize(value, maxDepth = 1) {
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
      let objectId = objectCache._objectToId.get(value);
      if (!objectId) {
        objectId = uuid();
        objectCache._objectToId.set(value, objectId);
        objectCache._idToObject.set(objectId, new WeakRef(value));
      }

      const result = { objectId };

      if (typeof value === 'function') {
        result.type = 'function';
        return result;
      }

      if (value instanceof Promise) {
        result.type = 'promise';
        return result;
      }

      if (Array.isArray(value)) {
        result.type = "array";
      } else {
        result.type = 'object';
      }

      if (maxDepth > 0) {
        result.value = [];
        for (let key of Object.keys(value)) {
          const serializedProperty = serialize(value[key], maxDepth - 1);

          if (Array.isArray(value)) {
            result.value.push(serializedProperty);
          } else {
            // TODO sadym: implement key serialisation.
            result.value.push([key, serialize(value[key], maxDepth - 1)]);
          }
        }
      }

      return result;
    } else {
      throw new Error('not yet implemented');
    }
  }

  function deserialize(value) {
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
      case 'promise':
      case 'array':
      case 'function':
      case 'object': {
        const weakRef = objectCache._idToObject.get(value.objectId);
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

  function uuid() {
    // TODO sadym: `crypto.randomUUID()` works only in secure context.
    // Find out a way to use`crypto.randomUUID`.
    return (Math.random() + '').substr(2)
      + '.' + (Math.random() + '').substr(2)
      + '.' + (Math.random() + '').substr(2);
  }

  function evaluate(script, args) {
      const deserializedArgs = args.map((arg) => deserialize(arg));
      const func = new Function(`return (\n${script}\n)`);
      const result = func.apply(null, deserializedArgs);
      const serializedResult = serialize(result);
      return serializedResult;
  };

  return {
    evaluate,
    serialize,
    deserialize
  }
})()
