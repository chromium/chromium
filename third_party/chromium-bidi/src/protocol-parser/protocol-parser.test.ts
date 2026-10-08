/**
 * Copyright 2026 Google LLC.
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
import {z} from 'zod';

import {parseObject} from './protocol-parser.js';

describe('parseObject', () => {
  const schema = z.object({
    context: z.string(),
  });

  it('should default goog:params to an empty object when no goog: properties are provided', () => {
    const result = parseObject({context: 'ctx-1', unknownKey: 123}, schema);
    assert.deepEqual(result, {
      context: 'ctx-1',
      'goog:params': {},
    });
  });

  it('should move goog:-prefixed properties into goog:params with the prefix stripped', () => {
    const result = parseObject(
      {
        context: 'ctx-1',
        'goog:shrinkToPage': true,
        'goog:fromSurface': false,
        unknownKey: 123,
      },
      schema,
    );
    assert.deepEqual(result, {
      context: 'ctx-1',
      'goog:params': {
        shrinkToPage: true,
        fromSurface: false,
      },
    });
  });
});
