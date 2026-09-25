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

import {beforeEach, describe, it} from 'node:test';
import {assert} from 'chai';
import type {Protocol} from 'devtools-protocol';
import sinon from 'sinon';

import type {CdpClient} from '../../../cdp/CdpClient.js';
import {
  InvalidArgumentException,
  type Storage,
  UnableToSetCookieException,
} from '../../../protocol/protocol.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import {StorageProcessor} from './StorageProcessor.js';

function createCdpCookie(
  name: string,
  partitionKey?: Protocol.Network.CookiePartitionKey,
): Protocol.Network.Cookie {
  return {
    name,
    value: 'val',
    domain: 'example.com',
    path: '/',
    expires: -1,
    size: name.length + 3,
    httpOnly: false,
    secure: true,
    session: true,
    priority: 'Medium',
    sourceScheme: 'Secure',
    sourcePort: 443,
    ...(partitionKey === undefined ? {} : {partitionKey}),
  };
}

describe('StorageProcessor', () => {
  let browserCdpClient: {sendCommand: sinon.SinonStub};
  let browsingContextStorage: sinon.SinonStubbedInstance<BrowsingContextStorage>;
  let processor: StorageProcessor;

  beforeEach(() => {
    browserCdpClient = {
      sendCommand: sinon.stub(),
    };
    browsingContextStorage = sinon.createStubInstance(BrowsingContextStorage);
    processor = new StorageProcessor(
      browserCdpClient as unknown as CdpClient,
      browsingContextStorage,
      undefined,
    );
  });

  describe('goog:hasCrossSiteAncestor validation', () => {
    it('should throw InvalidArgumentException when goog:hasCrossSiteAncestor is not a boolean', async () => {
      const invalidPartition = {
        type: 'storageKey',
        'goog:hasCrossSiteAncestor': 'true',
      } as unknown as Storage.PartitionDescriptor;

      for (const action of [
        () => processor.getCookies({partition: invalidPartition}),
        () => processor.deleteCookies({partition: invalidPartition}),
        () =>
          processor.setCookie({
            cookie: {
              name: 'foo',
              value: {type: 'string', value: 'bar'},
              domain: 'example.com',
            },
            partition: invalidPartition,
          }),
      ]) {
        try {
          await action();
          assert.fail('Expected InvalidArgumentException to be thrown');
        } catch (err) {
          assert.instanceOf(err, InvalidArgumentException);
        }
      }
    });
  });

  describe('getCookies', () => {
    const cookies = [
      createCdpCookie('unpartitioned'),
      createCdpCookie('same_site', {
        topLevelSite: 'https://example.com',
        hasCrossSiteAncestor: false,
      }),
      createCdpCookie('cross_site_ancestor', {
        topLevelSite: 'https://example.com',
        hasCrossSiteAncestor: true,
      }),
      createCdpCookie('other_origin_cross_site', {
        topLevelSite: 'https://other.example',
        hasCrossSiteAncestor: true,
      }),
    ];

    beforeEach(() => {
      browserCdpClient.sendCommand
        .withArgs('Storage.getCookies')
        .resolves({cookies});
    });

    it('should omit goog:hasCrossSiteAncestor from partitionKey and return both same-site and cross-site cookies when only sourceOrigin is specified', async () => {
      const result = await processor.getCookies({
        partition: {
          type: 'storageKey',
          sourceOrigin: 'https://example.com',
        },
      });

      assert.deepEqual(result.partitionKey, {
        userContext: 'default',
        sourceOrigin: 'https://example.com',
      });
      assert.deepEqual(
        result.cookies.map((c) => c.name),
        ['same_site', 'cross_site_ancestor'],
      );
    });

    it('should filter by both sourceOrigin and goog:hasCrossSiteAncestor when both are specified', async () => {
      const result = await processor.getCookies({
        partition: {
          type: 'storageKey',
          sourceOrigin: 'https://example.com',
          'goog:hasCrossSiteAncestor': true,
        },
      });

      assert.deepEqual(result.partitionKey, {
        userContext: 'default',
        sourceOrigin: 'https://example.com',
        'goog:hasCrossSiteAncestor': true,
      });
      assert.deepEqual(
        result.cookies.map((c) => c.name),
        ['cross_site_ancestor'],
      );
    });

    it('should filter by goog:hasCrossSiteAncestor alone when sourceOrigin is omitted', async () => {
      const result = await processor.getCookies({
        partition: {
          type: 'storageKey',
          'goog:hasCrossSiteAncestor': true,
        },
      });

      assert.deepEqual(result.partitionKey, {
        userContext: 'default',
        'goog:hasCrossSiteAncestor': true,
      });
      assert.deepEqual(
        result.cookies.map((c) => c.name),
        ['cross_site_ancestor', 'other_origin_cross_site'],
      );
    });
  });

  describe('deleteCookies', () => {
    const cookies = [
      createCdpCookie('same_site', {
        topLevelSite: 'https://example.com',
        hasCrossSiteAncestor: false,
      }),
      createCdpCookie('cross_site_ancestor', {
        topLevelSite: 'https://example.com',
        hasCrossSiteAncestor: true,
      }),
    ];

    beforeEach(() => {
      browserCdpClient.sendCommand
        .withArgs('Storage.getCookies')
        .resolves({cookies});
      browserCdpClient.sendCommand.withArgs('Storage.setCookies').resolves({});
    });

    it('should delete only cookies matching goog:hasCrossSiteAncestor', async () => {
      const result = await processor.deleteCookies({
        partition: {
          type: 'storageKey',
          sourceOrigin: 'https://example.com',
          'goog:hasCrossSiteAncestor': false,
        },
      });

      assert.deepEqual(result.partitionKey, {
        userContext: 'default',
        sourceOrigin: 'https://example.com',
        'goog:hasCrossSiteAncestor': false,
      });
      const setCookiesCall = browserCdpClient.sendCommand
        .withArgs('Storage.setCookies')
        .getCall(0);
      assert.isNotNull(setCookiesCall);
      assert.deepEqual(
        setCookiesCall.args[1].cookies.map(
          (c: Protocol.Network.CookieParam) => c.name,
        ),
        ['same_site'],
      );
    });
  });

  describe('setCookie', () => {
    beforeEach(() => {
      browserCdpClient.sendCommand.withArgs('Storage.setCookies').resolves({});
    });

    it('should set cookie with hasCrossSiteAncestor and return expanded partitionKey', async () => {
      const result = await processor.setCookie({
        cookie: {
          name: 'foo',
          value: {type: 'string', value: 'bar'},
          domain: 'example.com',
          secure: true,
        },
        partition: {
          type: 'storageKey',
          sourceOrigin: 'https://example.com',
          'goog:hasCrossSiteAncestor': true,
        },
      });

      assert.deepEqual(result.partitionKey, {
        userContext: 'default',
        sourceOrigin: 'https://example.com',
        'goog:hasCrossSiteAncestor': true,
      });
      const setCookiesCall = browserCdpClient.sendCommand
        .withArgs('Storage.setCookies')
        .getCall(0);
      assert.deepEqual(setCookiesCall.args[1].cookies[0].partitionKey, {
        topLevelSite: 'https://example.com',
        hasCrossSiteAncestor: true,
      });
    });

    it('should throw UnableToSetCookieException when goog:hasCrossSiteAncestor is provided without sourceOrigin', async () => {
      try {
        await processor.setCookie({
          cookie: {
            name: 'foo',
            value: {type: 'string', value: 'bar'},
            domain: 'example.com',
            secure: true,
          },
          partition: {
            type: 'storageKey',
            'goog:hasCrossSiteAncestor': true,
          },
        });
        assert.fail('Expected UnableToSetCookieException to be thrown');
      } catch (err) {
        assert.instanceOf(err, UnableToSetCookieException);
      }
    });
  });
});
