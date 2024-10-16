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
import {expect} from 'chai';

import {assert} from './assert.js';

describe('assert', () => {
  it('should not throw an error when the predicate is truthy', () => {
    expect(() => assert(true)).to.not.throw(Error);
    expect(() => assert(1)).to.not.throw(Error);
    expect(() => assert('hello')).to.not.throw(Error);
  });

  it('should throw an error when the predicate is falsy', () => {
    expect(() => assert(false)).to.throw(Error, 'Internal assertion failed.');
    expect(() => assert(0)).to.throw(Error, 'Internal assertion failed.');
    expect(() => assert('')).to.throw(Error, 'Internal assertion failed.');
    expect(() => assert(null)).to.throw(Error, 'Internal assertion failed.');
  });

  it('should throw an error when the predicate is undefined or null', () => {
    expect(() => assert(undefined)).to.throw(
      Error,
      'Internal assertion failed.',
    );
    expect(() => assert(null)).to.throw(Error, 'Internal assertion failed.');
  });
});
