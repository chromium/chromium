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
import sinon from 'sinon';

import {Deferred} from './Deferred.js';
import {LogType} from './log.js';
import {ProcessingQueue} from './ProcessingQueue.js';
import type {Result} from './result.js';

describe('ProcessingQueue', () => {
  it('should wait and call processor in order', async () => {
    const processor = sinon.stub().returns(Promise.resolve());
    const queue = new ProcessingQueue<number>(processor);
    const deferred1 = new Deferred<Result<number>>();
    const deferred2 = new Deferred<Result<number>>();
    const deferred3 = new Deferred<Result<number>>();

    queue.add(deferred1, '');
    await wait(1);
    sinon.assert.notCalled(processor);

    queue.add(deferred2, '');
    queue.add(deferred3, '');
    await wait(1);
    sinon.assert.notCalled(processor);

    deferred3.resolve({
      kind: 'success',
      value: 3,
    });
    deferred2.resolve({
      kind: 'success',
      value: 2,
    });
    await wait(1);
    sinon.assert.notCalled(processor);

    deferred1.resolve({
      kind: 'success',
      value: 1,
    });
    await wait(1);

    const processedValues = processor.getCalls().map((c) => c.firstArg);
    expect(processedValues).to.deep.equal([1, 2, 3]);
  });

  it('rejects should not stop processing with rejects from processor', async () => {
    const error = new Error('Processor reject');
    const processor = sinon.stub().returns(Promise.reject(error));
    const logger = sinon.spy();
    const queue = new ProcessingQueue<number>(processor, logger);
    const deferred = new Deferred<Result<number>>();

    queue.add(deferred, '');

    deferred.resolve({
      kind: 'success',
      value: 2,
    });
    await wait(1);

    // Assert processor was called with successful value.
    sinon.assert.calledOnceWithExactly(processor, 2);

    // Assert `_catch` was called for waiting entry and processor call.
    const loggerErrorValues = logger
      .getCalls()
      .filter((c) => c.args[0] === LogType.debugError)
      .map((c) => c.args[2]);
    expect(loggerErrorValues).to.deep.equal([error.message]);
  });

  it('rejects should not stop processing with rejects from queue', async () => {
    const error = new Error('Processor reject');
    const processor = sinon.stub().returns(Promise.resolve());
    const logger = sinon.spy();
    const queue = new ProcessingQueue<number>(processor, logger);
    const deferred1 = new Deferred<Result<number>>();
    const deferred2 = new Deferred<Result<number>>();

    queue.add(deferred1, '');
    queue.add(deferred2, '');

    deferred1.reject(error);
    deferred2.resolve({
      kind: 'success',
      value: 2,
    });
    await wait(1);

    // Assert processor was called with successful value.
    sinon.assert.calledOnceWithExactly(processor, 2);

    // Assert `_catch` was called for waiting entry and processor call.
    const loggerErrorValues = logger
      .getCalls()
      .filter((c) => c.args[0] === LogType.debugError)
      .map((c) => c.args[2]);
    expect(loggerErrorValues).to.deep.equal([error.message]);
  });

  it('rejects should not stop processing for queue events with result errors', async () => {
    const error = new Error('Processor reject');
    const processor = sinon.stub().returns(Promise.resolve());
    const logger = sinon.spy();
    const queue = new ProcessingQueue<number>(processor, logger);
    const deferred1 = new Deferred<Result<number>>();
    const deferred2 = new Deferred<Result<number>>();

    queue.add(deferred1, '');
    queue.add(deferred2, '');

    deferred1.resolve({
      kind: 'error',
      error,
    });
    deferred2.resolve({
      kind: 'success',
      value: 2,
    });
    await wait(1);

    // Assert processor was called with successful value.
    sinon.assert.calledOnceWithExactly(processor, 2);

    // Assert `_catch` was called for waiting entry and processor call.
    const loggerErrorValues = logger
      .getCalls()
      .filter((c) => c.args[0] === LogType.debugError)
      .map((c) => c.args[2]);
    expect(loggerErrorValues).to.deep.equal([error.message]);
  });

  it('processing starts over when new values are added', async () => {
    const processor = sinon.stub().returns(Promise.resolve());
    const queue = new ProcessingQueue<number>(processor);

    queue.add(
      Promise.resolve({
        kind: 'success',
        value: 1,
      }),
      '',
    );
    await wait(1);
    sinon.assert.calledOnceWithExactly(processor, 1);
    processor.reset();

    queue.add(
      Promise.resolve({
        kind: 'success',
        value: 2,
      }),
      '',
    );
    await wait(1);
    sinon.assert.calledOnceWithExactly(processor, 2);

    const processedValues = processor.getCalls().map((c) => c.firstArg);
    expect(processedValues).to.deep.equal([2]);
  });

  it('processing log name when starting to process new values', async () => {
    const processor = sinon.stub().returns(Promise.resolve());
    const logger = sinon.spy();
    const queue = new ProcessingQueue<number>(processor, logger);
    const eventNames = [
      'EventNameOne',
      'EventNameTwo',
      'EventNameThree',
    ] as const;
    queue.add(
      Promise.resolve({
        kind: 'success',
        value: 1,
      }),
      eventNames[0],
    );
    queue.add(
      Promise.resolve({
        kind: 'error',
        error: new Error('Error'),
      }),
      eventNames[1],
    );
    queue.add(Promise.reject(new Error('Error')), eventNames[2]);

    await wait(1);

    const processedValues = logger
      .getCalls()
      .filter((c) => c.args[0] === ProcessingQueue.LOGGER_PREFIX)
      .map((c) => c.args[2]);
    expect(processedValues).to.deep.equal(eventNames);
  });
});

function wait(timeout: number): Promise<void> {
  return new Promise((resolve) => {
    setTimeout(resolve, timeout);
  });
}
