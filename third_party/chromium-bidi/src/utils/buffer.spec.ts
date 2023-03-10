/**
 * Copyright 2022 Google LLC.
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

import * as chai from 'chai';
import * as sinon from 'sinon';

import {Buffer} from './buffer.js';

const expect = chai.expect;

describe('test Buffer', () => {
  it('should keep values', () => {
    const buffer = new Buffer<number>(2);
    expect(buffer.get()).to.deep.equal([]);
    buffer.add(1);
    expect(buffer.get()).to.deep.equal([1]);
    buffer.add(2);
    expect(buffer.get()).to.deep.equal([1, 2]);
    buffer.add(3);
    expect(buffer.get()).to.deep.equal([2, 3]);
  });
  it('should call "onRemoved"', () => {
    const onRemoved = sinon.mock();
    const buffer = new Buffer<number>(2, onRemoved);
    buffer.add(1);
    sinon.assert.notCalled(onRemoved);
    buffer.add(2);
    sinon.assert.notCalled(onRemoved);
    buffer.add(3);
    sinon.assert.calledOnceWithExactly(onRemoved, 1);
  });
});
