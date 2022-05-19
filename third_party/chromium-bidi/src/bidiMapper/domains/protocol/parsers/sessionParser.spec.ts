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

import * as chai from 'chai';
import { SessionParser } from './sessionParser';
import { BrowsingContext, Log } from '../bidiProtocolTypes';

const expect = chai.expect;

describe('test SubscribeParamsParser', function () {
  describe('parse', function () {
    it('valid params should be parsed', async function () {
      // noinspection JSPrimitiveTypeWrapperUsage
      const params = {
        contexts: ['some_context', new String('another_context')],
        events: [
          Log.LogEntryAddedEvent.method,
          new String(BrowsingContext.ContextCreatedEvent.method),
        ],
      };
      expect(SessionParser.SubscribeParamsParser.parse(params)).to.deep.equal(
        params
      );
    });

    it('params without contexts should be parsed', async function () {
      const params = {
        events: [Log.LogEntryAddedEvent.method],
      };
      expect(SessionParser.SubscribeParamsParser.parse(params)).to.deep.equal(
        params
      );
    });

    it('params with empty contexts and events should be parsed', async function () {
      const params = {
        contexts: [],
        events: [],
      };
      expect(SessionParser.SubscribeParamsParser.parse(params)).to.deep.equal(
        params
      );
    });

    it('params without events should not be parsed', async function () {
      const params = {
        contexts: ['some_context'],
      };
      expect(() => SessionParser.SubscribeParamsParser.parse(params))
        .to.throw()
        .with.property('error', 'invalid argument');
    });

    it('params with invalid events should not be parsed', async function () {
      const params = {
        contexts: ['some_context'],
        events: [42],
      };
      expect(() => SessionParser.SubscribeParamsParser.parse(params))
        .to.throw()
        .with.property('error', 'invalid argument');
    });

    it('params with invalid contexts should not be parsed', async function () {
      const params = {
        contexts: [42],
        events: ['event.1'],
      };
      expect(() => SessionParser.SubscribeParamsParser.parse(params))
        .to.throw()
        .with.property('error', 'invalid argument');
    });

    it('empty params should throw error', async function () {
      expect(() => SessionParser.SubscribeParamsParser.parse({}))
        .to.throw()
        .with.property('error', 'invalid argument');
    });
  });
});
