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

import {URLPattern} from './UrlPattern.js';

describe('UrlPattern', () => {
  // There's no need to exhaustively test the URLPattern class.
  // This is a basic smoke test just to ensure that the class is available and
  // properly exported.
  it('smoke test', () => {
    const pattern = new URLPattern({
      protocol: 'https',
      hostname: 'example.com',
    });
    expect(pattern.exec('https://example.com/')).to.be.ok;
    expect(pattern.exec('http://example.com')).to.be.null;
  });
});
