/*
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
import type {CdpClient} from '../../../cdp/CdpClient.js';
import {UnableToSetCookieException} from '../../../protocol/protocol.js';
import type {Storage, Network} from '../../../protocol/protocol.js';
import {assert} from '../../../utils/assert.js';
import type {LoggerFn} from '../../../utils/log.js';
import {LogType} from '../../../utils/log.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {NetworkProcessor} from '../network/NetworkProcessor.js';
import {bidiToCdpCookie, cdpToBiDiCookie} from '../network/NetworkUtils.js';

/**
 * Responsible for handling the `storage` domain.
 */
export class StorageProcessor {
  readonly #browserCdpClient: CdpClient;
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #logger: LoggerFn | undefined;

  constructor(
    browserCdpClient: CdpClient,
    browsingContextStorage: BrowsingContextStorage,
    logger: LoggerFn | undefined
  ) {
    this.#browsingContextStorage = browsingContextStorage;
    this.#browserCdpClient = browserCdpClient;
    this.#logger = logger;
  }

  async deleteCookies(
    params: Storage.DeleteCookiesParameters
  ): Promise<Storage.DeleteCookiesResult> {
    const partitionKey = this.#expandStoragePartitionSpec(params.partition);

    const cdpResponse = await this.#browserCdpClient.sendCommand(
      'Storage.getCookies',
      {
        browserContextId: partitionKey.userContext,
      }
    );

    const cdpCookiesToDelete = cdpResponse.cookies
      .filter(
        // CDP's partition key is the source origin. If the request specifies the
        // `sourceOrigin` partition key, only cookies with the requested source origin
        // are returned.
        (c) =>
          partitionKey.sourceOrigin === undefined ||
          c.partitionKey === partitionKey.sourceOrigin
      )
      .filter((cdpCookie) => {
        const bidiCookie = cdpToBiDiCookie(cdpCookie);
        return this.#matchCookie(bidiCookie, params.filter);
      })
      .map((cookie) => ({
        ...cookie,
        // Set expiry to pass date to delete the cookie.
        expires: 1,
      }));

    await this.#browserCdpClient.sendCommand('Storage.setCookies', {
      cookies: cdpCookiesToDelete,
      browserContextId: partitionKey.userContext,
    });
    return {
      partitionKey,
    };
  }

  async getCookies(
    params: Storage.GetCookiesParameters
  ): Promise<Storage.GetCookiesResult> {
    const partitionKey = this.#expandStoragePartitionSpec(params.partition);

    const cdpResponse = await this.#browserCdpClient.sendCommand(
      'Storage.getCookies',
      {
        browserContextId: partitionKey.userContext,
      }
    );

    const filteredBiDiCookies = cdpResponse.cookies
      .filter(
        // CDP's partition key is the source origin. If the request specifies the
        // `sourceOrigin` partition key, only cookies with the requested source origin
        // are returned.
        (c) =>
          partitionKey.sourceOrigin === undefined ||
          c.partitionKey === partitionKey.sourceOrigin
      )
      .map((c) => cdpToBiDiCookie(c))
      .filter((c) => this.#matchCookie(c, params.filter));

    return {
      cookies: filteredBiDiCookies,
      partitionKey,
    };
  }

  async setCookie(
    params: Storage.SetCookieParameters
  ): Promise<Storage.SetCookieResult> {
    const partitionKey = this.#expandStoragePartitionSpec(params.partition);
    const cdpCookie = bidiToCdpCookie(params, partitionKey);

    try {
      await this.#browserCdpClient.sendCommand('Storage.setCookies', {
        cookies: [cdpCookie],
        browserContextId: partitionKey.userContext,
      });
    } catch (e: any) {
      this.#logger?.(LogType.debugError, e);
      throw new UnableToSetCookieException(e.toString());
    }
    return {
      partitionKey,
    };
  }

  #expandStoragePartitionSpecByBrowsingContext(
    descriptor: Storage.BrowsingContextPartitionDescriptor
  ): Storage.PartitionKey {
    const browsingContextId: string = descriptor.context;
    const browsingContext =
      this.#browsingContextStorage.getContext(browsingContextId);
    // https://w3c.github.io/webdriver-bidi/#associated-storage-partition.
    // Each browsing context also has an associated storage partition, which is the
    // storage partition it uses to persist data. In Chromium it's a `BrowserContext`
    // which maps to BiDi `UserContext`.
    return {
      userContext:
        browsingContext.userContext === 'default'
          ? undefined
          : browsingContext.userContext,
    };
  }

  #expandStoragePartitionSpecByStorageKey(
    descriptor: Storage.StorageKeyPartitionDescriptor
  ): Storage.PartitionKey {
    const unsupportedPartitionKeys = new Map<string, string>();
    let sourceOrigin = descriptor.sourceOrigin;
    if (sourceOrigin !== undefined) {
      const url = NetworkProcessor.parseUrlString(sourceOrigin);
      if (url.origin === 'null') {
        // Origin `null` is a special case for local pages.
        sourceOrigin = url.origin;
      } else {
        // Port is not supported in CDP Cookie's `partitionKey`, so it should be stripped
        // from the requested source origin.
        sourceOrigin = `${url.protocol}//${url.hostname}`;
      }
    }

    const userContext =
      descriptor.userContext === 'default' ? undefined : descriptor.userContext;

    // Partition spec is a storage partition.
    // Let partition key be partition spec.
    for (const [key, value] of Object.entries(descriptor)) {
      if (
        key !== undefined &&
        value !== undefined &&
        !['type', 'sourceOrigin', 'userContext'].includes(key)
      ) {
        unsupportedPartitionKeys.set(key, value);
      }
    }

    if (unsupportedPartitionKeys.size > 0) {
      this.#logger?.(
        LogType.debugInfo,
        `Unsupported partition keys: ${JSON.stringify(
          Object.fromEntries(unsupportedPartitionKeys)
        )}`
      );
    }

    return {
      ...(sourceOrigin === undefined ? {} : {sourceOrigin}),
      ...(userContext === undefined ? {} : {userContext}),
    };
  }

  #expandStoragePartitionSpec(
    partitionSpec: Storage.PartitionDescriptor | undefined
  ): Storage.PartitionKey {
    if (partitionSpec === undefined) {
      return {};
    }
    if (partitionSpec.type === 'context') {
      return this.#expandStoragePartitionSpecByBrowsingContext(partitionSpec);
    }
    assert(partitionSpec.type === 'storageKey', 'Unknown partition type');
    return this.#expandStoragePartitionSpecByStorageKey(partitionSpec);
  }

  #matchCookie(cookie: Network.Cookie, filter?: Storage.CookieFilter): boolean {
    if (filter === undefined) {
      return true;
    }
    return (
      (filter.domain === undefined || filter.domain === cookie.domain) &&
      (filter.name === undefined || filter.name === cookie.name) &&
      // `value` contains fields `type` and `value`.
      (filter.value === undefined ||
        (filter.value.type === cookie.value.type &&
          filter.value.value === cookie.value.value)) &&
      (filter.path === undefined || filter.path === cookie.path) &&
      (filter.size === undefined || filter.size === cookie.size) &&
      (filter.httpOnly === undefined || filter.httpOnly === cookie.httpOnly) &&
      (filter.secure === undefined || filter.secure === cookie.secure) &&
      (filter.sameSite === undefined || filter.sameSite === cookie.sameSite) &&
      (filter.expiry === undefined || filter.expiry === cookie.expiry)
    );
  }
}
