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

import {Deferred} from './Deferred.js';

describe('Deferred', () => {
  describe('isFinished', () => {
    it('resolve', async () => {
      const deferred = new Deferred<string>();
      const deferredThen = deferred.then((v) => v);
      deferred.catch((e) => {
        throw e;
      });

      assert.isFalse(deferred.isFinished);

      deferred.resolve('done');
      assert.isTrue(deferred.isFinished);

      assert.equal(await deferredThen, 'done');
    });

    it('reject', async () => {
      const deferred = new Deferred<string>();
      const deferredThen = deferred.then((v) => v);
      const deferredCatch = deferred.catch((e) => e);

      assert.isFalse(deferred.isFinished);

      deferred.reject(new Error('some error'));
      assert.isTrue(deferred.isFinished);

      const error = await deferredThen.catch((e: Error) => e);
      assert.propertyVal(error, 'message', 'some error');
      await deferredCatch;
      assert.instanceOf(await deferredCatch, Error);
      assert.propertyVal(await deferredCatch, 'message', 'some error');
    });

    it('finally', async () => {
      const deferred = new Deferred<string>();
      const deferredFinally = deferred.finally(() => {
        // Intentionally empty.
      });

      assert.isFalse(deferred.isFinished);

      deferred.resolve('done');
      assert.isTrue(deferred.isFinished);

      assert.equal(await deferredFinally, 'done');
    });

    describe('result', () => {
      it('should throw if not finished', () => {
        const deferred = new Deferred<string>();
        assert.throws(() => {
          deferred.result;
        }, 'Deferred is not finished yet');
      });

      it('should return when finished', () => {
        const deferred = new Deferred<string>();
        deferred.resolve('done');
        assert.equal(deferred.result, 'done');
      });
    });
  });
});
