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

import {uuidv4} from './uuid.js';

// These tests do not run in the browser, therefore their value is limited.
describe('uuidv4', () => {
  it('should generate an UUID in the correct format', () => {
    expect(uuidv4()).match(
      /^[0-9A-F]{8}-[0-9A-F]{4}-4[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$/i,
    );
  });

  it('subsequent calls should yield different UUIDs', () => {
    const id1 = uuidv4();
    const id2 = uuidv4();
    expect(id1).to.not.equal(id2);
  });
});
