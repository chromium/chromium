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

import {InvalidArgumentException} from '../protocol/protocol.js';

import {GoogBrowsingContext, GoogCdp, parseObject} from './protocol-parser.js';

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

describe('GoogCdp', () => {
  describe('parseSendCommandRequest', () => {
    it('should parse a CDP command', () => {
      const result: unknown = GoogCdp.parseSendCommandRequest({
        method: 'Page.navigate',
        params: {url: 'about:blank'},
        session: 'session-1',
      });
      assert.deepEqual(result, {
        method: 'Page.navigate',
        params: {url: 'about:blank'},
        session: 'session-1',
        'goog:params': {},
      });
    });

    it('should parse a CDP command without optional params and session', () => {
      const result: unknown = GoogCdp.parseSendCommandRequest({
        method: 'Page.enable',
      });
      assert.deepEqual(result, {
        method: 'Page.enable',
        'goog:params': {},
      });
    });

    it('should throw if the method is missing', () => {
      assert.throws(
        () => GoogCdp.parseSendCommandRequest({params: {}}),
        InvalidArgumentException,
      );
    });

    it('should throw if the method is not a string', () => {
      assert.throws(
        () => GoogCdp.parseSendCommandRequest({method: 123}),
        InvalidArgumentException,
      );
    });

    it('should throw if the params are not a record', () => {
      for (const params of [42, null, []]) {
        assert.throws(
          () =>
            GoogCdp.parseSendCommandRequest({method: 'Page.enable', params}),
          InvalidArgumentException,
        );
      }
    });

    it('should throw if the session is not a string', () => {
      assert.throws(
        () =>
          GoogCdp.parseSendCommandRequest({
            method: 'Page.enable',
            session: 123,
          }),
        InvalidArgumentException,
      );
    });
  });

  describe('parseGetSessionRequest', () => {
    it('should parse the context', () => {
      const result: unknown = GoogCdp.parseGetSessionRequest({
        context: 'context-1',
      });
      assert.deepEqual(result, {context: 'context-1', 'goog:params': {}});
    });

    it('should throw if the context is missing', () => {
      assert.throws(
        () => GoogCdp.parseGetSessionRequest({}),
        InvalidArgumentException,
      );
    });

    it('should throw if the context is not a string', () => {
      assert.throws(
        () => GoogCdp.parseGetSessionRequest({context: 123}),
        InvalidArgumentException,
      );
    });
  });

  describe('parseResolveRealmRequest', () => {
    it('should parse the realm', () => {
      const result: unknown = GoogCdp.parseResolveRealmRequest({
        realm: 'realm-1',
      });
      assert.deepEqual(result, {realm: 'realm-1', 'goog:params': {}});
    });

    it('should throw if the realm is missing', () => {
      assert.throws(
        () => GoogCdp.parseResolveRealmRequest({}),
        InvalidArgumentException,
      );
    });

    it('should throw if the realm is not a string', () => {
      assert.throws(
        () => GoogCdp.parseResolveRealmRequest({realm: 1}),
        InvalidArgumentException,
      );
    });
  });
});

describe('GoogBrowsingContext', () => {
  describe('parseGetAccessibilityTreeParams', () => {
    it('should parse the context', () => {
      const result: unknown =
        GoogBrowsingContext.parseGetAccessibilityTreeParams({
          context: 'context-1',
        });
      assert.deepEqual(result, {context: 'context-1', 'goog:params': {}});
    });

    it('should throw if the context is missing', () => {
      assert.throws(
        () => GoogBrowsingContext.parseGetAccessibilityTreeParams({}),
        InvalidArgumentException,
      );
    });

    it('should throw if the context is not a string', () => {
      assert.throws(
        () =>
          GoogBrowsingContext.parseGetAccessibilityTreeParams({context: 123}),
        InvalidArgumentException,
      );
    });
  });
});
