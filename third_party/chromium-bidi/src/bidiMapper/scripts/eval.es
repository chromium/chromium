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

      // TODO: add unit test handling Node class properly.
      if (typeof Node !== "undefined" && value instanceof Node) {
        result.type = 'node';

        // TODO: current implementation relies on `maxDepth`, while specification relies on
        // `node details`. Decide which way is better and fix specification or implementation.
        if (maxDepth > 0) {
          result.value = {
            nodeType: value.nodeType,
            childNodeCount: value.childNodes.length
          }
          if(value.nodeValue!==null)
            result.nodeValue = value.nodeValue;

          // TODO: current implementation serializes `childNodes` instead of `children`. Decide
          // which way is better and fix specification or implementation.
          result.children = [];
          for (let childNode of value.childNodes) {
            const c = serialize(childNode, maxDepth-1);
            result.children.push(c);
          }

          if (value instanceof Element){
            result.attributes = {};
            for(let attribute of value.attributes){
              result.attributes[attribute.name] = attribute.value;
            }
            if(value.shadowRoot!==null)
              result.shadowRoot = serialize(value.shadowRoot, maxDepth-1);
          }
        }

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
            // TODO sadym: implement key serialization.
            result.value.push([key, serialize(value[key], maxDepth - 1)]);
          }
        }
      }

      return result;
    } else {
      throw new Error('not yet implemented');
    }
  }

  function deserialize(serializedValue) {
    if (serializedValue.hasOwnProperty('objectId')) {
      const weakRef = objectCache._idToObject.get(serializedValue.objectId);
      if (!weakRef) {
        throw new Error('unknown object reference');
      }

      const obj = weakRef.deref();
      if (!obj) {
        throw new Error('stale object reference');
      }

      return obj;
    }

    switch (serializedValue.type) {
      case 'undefined': {
        return undefined;
      }
      case 'null': {
        return null;
      }
      case 'string': {
        return serializedValue.value;
      }
      case 'number': {
        if (serializedValue.value === 'NaN') {
          return NaN;
        } else if (serializedValue.value === '-0') {
          return -0;
        } else if (serializedValue.value === '+Infinity') {
          return +Infinity;
        } else if (serializedValue.value === '-Infinity') {
          return -Infinity;
        } else {
          return Number(serializedValue.value);
        }
      }
      case 'boolean': {
        return serializedValue.value;
      }
      case 'array': {
        const result = [];
        for (let val of serializedValue.value) {
          result.push(deserialize(val));
        }
        return result;
      }
      case 'object': {
        const result = {};
        for (let val of serializedValue.value) {
          // TODO sadym: implement key deserialization.
          result[val[0]] = deserialize(val[1]);
        }
        return result;
      }
      case 'promise':
      case 'function':
        throw new Error(`type ${serializedValue.type} cannot be deserialized`);
      default:
        throw new Error(`deserialization of ${serializedValue.type} is not yet implemented`);

    }
  }

  function uuid() {
    // TODO sadym: `crypto.randomUUID()` works only in secure context.
    // Find out a way to use`crypto.randomUUID`.
    return (Math.random() + '').substr(2)
      + '.' + (Math.random() + '').substr(2)
      + '.' + (Math.random() + '').substr(2);
  }

  return {
    serialize,
    deserialize
  }
})()
