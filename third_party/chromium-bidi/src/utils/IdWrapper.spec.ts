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

import {expect} from 'chai';

import {IdWrapper} from './IdWrapper.js';

describe('IdWrapper', () => {
  it('wraps value with unique id', () => {
    const wrapper1 = new IdWrapper();
    const wrapper2 = new IdWrapper();

    expect(wrapper1.id).to.be.an('number');
    expect(wrapper2.id).to.be.an('number');

    expect(wrapper2.id).to.be.greaterThan(wrapper1.id);
  });
});
