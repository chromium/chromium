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

import * as chai from 'chai';
import {expect} from 'chai';
import chaiAsPromised from 'chai-as-promised';

import {Deferred} from './Deferred.js';

chai.use(chaiAsPromised);

describe('Deferred', () => {
  describe('isFinished', () => {
    it('resolve', async () => {
      const deferred = new Deferred<string>();
      const deferredThen = deferred.then((v) => v);
      deferred.catch((e) => {
        throw e;
      });

      expect(deferred.isFinished).to.be.false;

      deferred.resolve('done');
      expect(deferred.isFinished).to.be.true;

      await expect(deferredThen).to.eventually.equal('done');
    });

    it('reject', async () => {
      const deferred = new Deferred<string>();
      const deferredThen = deferred.then((v) => v);
      const deferredCatch = deferred.catch((e) => e);

      expect(deferred.isFinished).to.be.false;

      deferred.reject('some error');
      expect(deferred.isFinished).to.be.true;

      await expect(deferredThen).to.eventually.be.rejectedWith('some error');
      await expect(deferredCatch).to.eventually.equal('some error');
    });

    it('finally', async () => {
      const deferred = new Deferred<string>();
      const deferredFinally = deferred.finally(() => {
        // Intentionally empty.
      });

      expect(deferred.isFinished).to.be.false;

      deferred.resolve('done');
      expect(deferred.isFinished).to.be.true;

      await expect(deferredFinally).to.eventually.equal('done');
    });

    describe('result', () => {
      it('should throw if not finished', () => {
        const deferred = new Deferred<string>();
        expect(() => {
          deferred.result;
        }).to.throw('Deferred is not finished yet');
      });

      it('should return when finished', () => {
        const deferred = new Deferred<string>();
        deferred.resolve('done');
        expect(deferred.result).to.equal('done');
      });
    });
  });
});
