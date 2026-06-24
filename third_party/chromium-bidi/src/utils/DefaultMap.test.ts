/**
 * Copyright 2023 Google LLC.
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

import {describe, it} from 'node:test';
import {assert} from 'chai';

import {DefaultMap} from './DefaultMap.js';

describe('DefaultMap', () => {
  it('returns the default value when key does not exist', () => {
    const defaultValue = 42;

    const cutenessMap = new DefaultMap<string, number>(
      () => defaultValue,
      [['dog', 100]],
    );

    assert.deepEqual(cutenessMap.get('dog'), 100);
    assert.deepEqual(cutenessMap.get('cat'), defaultValue);

    assert.deepEqual(Array.from(cutenessMap.keys()), ['dog', 'cat']);
    assert.deepEqual(Array.from(cutenessMap.values()), [100, defaultValue]);
  });

  it('sets and gets properly', () => {
    const cutenessMap = new DefaultMap<string, number>(() => 0);

    cutenessMap.set('cat', 50);
    assert.deepEqual(cutenessMap.get('cat'), 50);

    assert.deepEqual(Array.from(cutenessMap.keys()), ['cat']);
    assert.deepEqual(Array.from(cutenessMap.values()), [50]);
  });
});
