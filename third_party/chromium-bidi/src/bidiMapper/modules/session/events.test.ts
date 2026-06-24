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

import {ChromiumBidi} from '../../../protocol/protocol.js';

import {assertSupportedEvent, isCdpEvent} from './events.js';

const CDP_EVENTS = [
  'goog:cdp',
  'goog:cdp.event',
  'goog:cdp.Debugger.breakpointResolved',
];
const NON_CDP_EVENTS = ['log', 'log.entryAdded'];

describe('event', () => {
  describe('isCdpEvent', () => {
    for (const cdpEvent of CDP_EVENTS) {
      it(`should return true for CDP event '${cdpEvent}'`, () => {
        assert.isTrue(isCdpEvent(cdpEvent));
      });
    }

    for (const nonCdpEvent of NON_CDP_EVENTS) {
      it(`should return false for non-CDP event '${nonCdpEvent}'`, () => {
        assert.isFalse(isCdpEvent(nonCdpEvent));
      });
    }
  });

  describe('assertSupportedEvent', () => {
    it('should throw for unknown events', () => {
      assert.throws(() => {
        assertSupportedEvent('unknown');
        assertSupportedEvent('cdp.Debugger.breakpointResolved');
        assertSupportedEvent(ChromiumBidi.Log.EventNames.LogEntryAdded);
      }, 'Unknown event: unknown');
    });

    it('should not throw for known CDP events', () => {
      assert.doesNotThrow(() => {
        assertSupportedEvent('goog:cdp.Debugger.breakpointResolved');
        assertSupportedEvent(ChromiumBidi.Log.EventNames.LogEntryAdded);
      });
    });
  });
});
