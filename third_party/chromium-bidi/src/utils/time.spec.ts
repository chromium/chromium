/**
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

import {getTimestamp} from './time.js';

describe('getTimestamp', () => {
  it('should return a number', () => {
    const timestamp = getTimestamp();
    expect(timestamp).to.be.a('number');
  });

  it(`should return a timestamp between '2020-01-01 00:00:00' and '2100-01-01 00:00:00'`, () => {
    const timestamp = getTimestamp();
    // Using a large delta to account for execution time and clock drift.
    // In a real test, consider mocking the underlying time source.
    expect(timestamp).to.be.within(1577833200000, 4102441200000);
  });
});
