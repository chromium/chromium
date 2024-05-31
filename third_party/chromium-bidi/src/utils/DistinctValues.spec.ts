/*
 * Copyright 2024 Google LLC.
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

import {expect} from 'chai';

import {deterministicJSONStringify, distinctValues} from './DistinctValues.js';

describe('functions from DistinctValues.ts', () => {
  describe('deterministicJSONStringify', () => {
    it('should sort properties alphabetically', () => {
      const data = {c: 3, b: 2, a: 1};
      const expected = '{"a":1,"b":2,"c":3}';
      const result = deterministicJSONStringify(data);
      expect(result).to.equal(expected);
    });

    it('should handle arrays (preserving order)', () => {
      const data = [3, 1, 2];
      const expected = '[3,1,2]';
      const result = deterministicJSONStringify(data);
      expect(result).to.equal(expected);
    });

    describe('should handling data type', () => {
      it('string', () => {
        const data = 'Hello';
        const expected = '"Hello"';
        const result = deterministicJSONStringify(data);
        expect(result).to.equal(expected);
      });

      it('number', () => {
        const data = 42;
        const expected = '42';
        const result = deterministicJSONStringify(data);
        expect(result).to.equal(expected);
      });

      it('boolean', () => {
        const data = true;
        const expected = 'true';
        const result = deterministicJSONStringify(data);
        expect(result).to.equal(expected);
      });

      it('null', () => {
        const data = null;
        const expected = 'null';
        const result = deterministicJSONStringify(data);
        expect(result).to.equal(expected);
      });

      it('undefined', () => {
        const data = undefined;
        const expected = undefined;
        const result = deterministicJSONStringify(data);
        expect(result).to.equal(expected);
      });
    });

    describe('should handling different nested data types', () => {
      it('string', () => {
        const data = {nested: 'Hello'};
        const expected = '{"nested":"Hello"}';
        const result = deterministicJSONStringify(data);
        expect(result).to.equal(expected);
      });

      it('number', () => {
        const data = {nested: 42};
        const expected = '{"nested":42}';
        const result = deterministicJSONStringify(data);
        expect(result).to.equal(expected);
      });

      it('boolean', () => {
        const data = {nested: true};
        const expected = '{"nested":true}';
        const result = deterministicJSONStringify(data);
        expect(result).to.equal(expected);
      });

      it('null', () => {
        const data = {nested: null};
        const expected = '{"nested":null}';
        const result = deterministicJSONStringify(data);
        expect(result).to.equal(expected);
      });

      it('undefined', () => {
        const data = {nested: undefined};
        const expected = '{}';
        const result = deterministicJSONStringify(data);
        expect(result).to.equal(expected);
      });
    });
  });

  describe('distinctValues', () => {
    it('should return distinct primitive values', () => {
      const arr = [1, 2, 2, 3, 'hello', 'hello', true, true];
      const expected = [1, 2, 3, 'hello', true];
      const result = distinctValues(arr);
      expect(result).to.have.deep.members(expected); // Check for deep equality (order doesn't matter)
    });

    it('should treat deep equal objects as the same', () => {
      const arr = [{a: 1}, {a: 1}, {b: 2}, {a: 1, b: 2}, {b: 2}, {b: 2, a: 1}];
      const expected = [{a: 1}, {b: 2}, {a: 1, b: 2}];
      const result = distinctValues(arr);
      expect(result).to.have.deep.members(expected);
    });

    it('should handle nested objects', () => {
      const arr = [
        {a: 1, nested: {c: 3, d: 4}},
        {a: 1, nested: {d: 4, c: 3}}, // Same as the first
        {b: 2, nested: {d: 4}},
      ];
      const expected = [
        {a: 1, nested: {c: 3, d: 4}},
        {b: 2, nested: {d: 4}},
      ];
      const result = distinctValues(arr);
      expect(result).to.have.deep.members(expected);
    });

    it('should handle mixed data types', () => {
      const arr = [1, 'hello', {a: 1}, true, {a: 1}, 'hello'];
      const expected = [1, 'hello', {a: 1}, true];
      const result = distinctValues(arr);
      expect(result).to.have.deep.members(expected);
    });
  });
});
