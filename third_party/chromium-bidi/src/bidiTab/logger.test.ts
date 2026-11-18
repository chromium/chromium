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
import {afterEach, describe, it} from 'node:test';
import {assert} from 'chai';
import sinon from 'sinon';

import {LogType} from '../utils/log.js';

import {log} from './logger.js';

describe('logger', () => {
  const originalWindow = globalThis.window;

  afterEach(() => {
    if (originalWindow === undefined) {
      // @ts-expect-error restoring undefined window in Node environment
      delete globalThis.window;
    } else {
      globalThis.window = originalWindow;
    }
  });

  it('returns undefined when window is not defined', () => {
    assert.isUndefined(log(LogType.debugInfo));
  });

  it('returns undefined when sendDebugMessage is not defined', () => {
    globalThis.window = {} as Window & typeof globalThis;

    assert.isUndefined(log(LogType.debugInfo));
  });

  it('returns undefined for BiDi logs even when sendDebugMessage is defined', () => {
    const sendDebugMessage = sinon.spy();
    globalThis.window = {sendDebugMessage} as unknown as Window &
      typeof globalThis;

    assert.isUndefined(log(LogType.bidi));
    assert.isUndefined(log(`${LogType.bidi}:SEND`));
    sinon.assert.notCalled(sendDebugMessage);
  });

  it('forwards non-BiDi logs to sendDebugMessage', () => {
    const sendDebugMessage = sinon.spy();
    globalThis.window = {sendDebugMessage} as unknown as Window &
      typeof globalThis;

    log(LogType.debugInfo)?.('launched', {ok: true});

    sinon.assert.calledOnceWithExactly(
      sendDebugMessage,
      JSON.stringify(
        {
          logType: LogType.debugInfo,
          messages: ['launched', {ok: true}],
        },
        null,
        2,
      ),
    );
  });
});
