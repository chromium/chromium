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
import {assert as chaiAssert} from 'chai';

import {assert} from './assert.js';

describe('assert', () => {
  it('should not throw an error when the predicate is truthy', () => {
    chaiAssert.doesNotThrow(() => assert(true), Error);
    chaiAssert.doesNotThrow(() => assert(1), Error);
    chaiAssert.doesNotThrow(() => assert('hello'), Error);
  });

  it('should throw an error when the predicate is falsy', () => {
    chaiAssert.throws(() => assert(false), Error, 'Internal assertion failed.');
    chaiAssert.throws(() => assert(0), Error, 'Internal assertion failed.');
    chaiAssert.throws(() => assert(''), Error, 'Internal assertion failed.');
    chaiAssert.throws(() => assert(null), Error, 'Internal assertion failed.');
  });

  it('should throw an error when the predicate is undefined or null', () => {
    chaiAssert.throws(
      () => assert(undefined),
      Error,
      'Internal assertion failed.',
    );
    chaiAssert.throws(() => assert(null), Error, 'Internal assertion failed.');
  });
});
