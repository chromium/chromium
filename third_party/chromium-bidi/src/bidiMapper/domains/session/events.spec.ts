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

import {ChromiumBidi} from '../../../protocol/protocol.js';

import {assertSupportedEvent, isCdpEvent} from './events.js';

describe('event', () => {
  describe('isCdpEvent', () => {
    it('should return true for CDP events', () => {
      expect(isCdpEvent('cdp')).to.be.true;
      expect(isCdpEvent('cdp.event')).to.be.true;
    });

    it('should return false for non-CDP events', () => {
      expect(isCdpEvent('log')).to.be.false;
      expect(isCdpEvent('log.entryAdded')).to.be.false;
    });
  });

  describe('assertSupportedEvent', () => {
    it('should throw for unknown events', () => {
      expect(() => {
        assertSupportedEvent('unknown');
      }).to.throw('Unknown event: unknown');
    });

    it('should not throw for known events', () => {
      expect(() => {
        assertSupportedEvent('cdp.Debugger.breakpointResolved');
        assertSupportedEvent(ChromiumBidi.Log.EventNames.LogEntryAdded);
      }).to.not.throw();
    });
  });
});
