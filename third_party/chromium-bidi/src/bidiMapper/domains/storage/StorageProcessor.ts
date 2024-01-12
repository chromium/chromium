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
import type {Protocol} from 'devtools-protocol';

import type {CdpClient} from '../../../cdp/CdpClient.js';
import type {Storage} from '../../../protocol/protocol.js';
import {
  InvalidArgumentException,
  Network,
  UnableToSetCookieException,
  UnderspecifiedStoragePartitionException,
  UnsupportedOperationException,
} from '../../../protocol/protocol.js';
import {assert} from '../../../utils/assert.js';
import type {LoggerFn} from '../../../utils/log.js';
import {LogType} from '../../../utils/log.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {NetworkProcessor} from '../network/NetworkProcessor.js';

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

  async getCookies(
    params: Storage.GetCookiesParameters
  ): Promise<Storage.GetCookiesResult> {
    const partitionKey = this.#expandStoragePartitionSpec(params.partition);

    const cdpResponse = await this.#browserCdpClient.sendCommand(
      'Storage.getCookies',
      {}
    );

    const filteredBiDiCookies = cdpResponse.cookies
      .filter(
        // CDP's partition key is the source origin.
        (c) =>
          c.partitionKey === undefined ||
          c.partitionKey === partitionKey.sourceOrigin
      )
      .map((c) => this.#cdpToBiDiCookie(c))
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
    const cdpCookie = this.#bidiToCdpCookie(params, partitionKey);

    try {
      await this.#browserCdpClient.sendCommand('Storage.setCookies', {
        cookies: [cdpCookie],
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
    const url = NetworkProcessor.parseUrlString(browsingContext?.url ?? '');
    // Cookie origin should not contain the port.
    // Origin `null` is a special case for local pages.
    const sourceOrigin =
      url.origin === 'null' ? url.origin : `${url.protocol}//${url.hostname}`;

    return {
      sourceOrigin,
    };
  }

  #expandStoragePartitionSpecByStorageKey(
    descriptor: Storage.StorageKeyPartitionDescriptor
  ): Storage.PartitionKey {
    let sourceOrigin: string | undefined = undefined;

    if (descriptor.sourceOrigin !== undefined) {
      sourceOrigin = descriptor.sourceOrigin;
    }

    if (sourceOrigin === undefined) {
      throw new UnderspecifiedStoragePartitionException(
        '"sourceOrigin" should be set'
      );
    }

    const unsupportedPartitionKeys = new Map<string, string>();

    // Partition spec is a storage partition.
    // Let partition key be partition spec.
    for (const [key, value] of Object.entries(descriptor)) {
      if (
        key !== undefined &&
        value !== undefined &&
        !['type', 'sourceOrigin'].includes(key)
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
      sourceOrigin,
    };
  }

  #expandStoragePartitionSpec(
    partitionSpec: Storage.PartitionDescriptor | undefined
  ): Storage.PartitionKey {
    if (partitionSpec === undefined) {
      throw new UnderspecifiedStoragePartitionException(
        'partition should be set'
      );
    }
    if (partitionSpec.type === 'context') {
      return this.#expandStoragePartitionSpecByBrowsingContext(partitionSpec);
    }
    assert(partitionSpec.type === 'storageKey', 'Unknown partition type');
    return this.#expandStoragePartitionSpecByStorageKey(partitionSpec);
  }

  #bidiToCdpCookie(
    params: Storage.SetCookieParameters,
    partitionKey: Storage.PartitionKey
  ): Protocol.Network.CookieParam {
    if (params.cookie.value.type !== 'string') {
      // CDP supports only string values in cookies.
      throw new UnsupportedOperationException(
        'Only string cookie values are supported'
      );
    }
    const deserializedValue = params.cookie.value.value;
    return {
      name: params.cookie.name,
      value: deserializedValue,
      domain: params.cookie.domain,
      path: params.cookie.path ?? '/',
      secure: params.cookie.secure ?? false,
      httpOnly: params.cookie.httpOnly ?? false,
      // CDP's `partitionKey` is the BiDi's `partition.sourceOrigin`.
      partitionKey: partitionKey.sourceOrigin,
      ...(params.cookie.expiry !== undefined && {
        expires: params.cookie.expiry,
      }),
      ...(params.cookie.sameSite !== undefined && {
        sameSite: StorageProcessor.#sameSiteBiDiToCdp(params.cookie.sameSite),
      }),
      // TODO: extend with CDP-specific properties with `goog:` prefix after
      //  https://github.com/w3c/webdriver-bidi/pull/637
      //  * session: boolean;
      //  * priority: CookiePriority;
      //  * sameParty: boolean;
      //  * sourceScheme: CookieSourceScheme;
      //  * sourcePort: integer;
      //  * partitionKey?: string;
      //  * partitionKeyOpaque?: boolean;
    };
  }

  #cdpToBiDiCookie(cookie: Protocol.Network.Cookie): Network.Cookie {
    return {
      name: cookie.name,
      value: {type: 'string', value: cookie.value},
      domain: cookie.domain,
      path: cookie.path,
      size: cookie.size,
      httpOnly: cookie.httpOnly,
      secure: cookie.secure,
      sameSite:
        cookie.sameSite === undefined
          ? Network.SameSite.None
          : StorageProcessor.#sameSiteCdpToBiDi(cookie.sameSite),
      ...(cookie.expires >= 0 ? {expiry: cookie.expires} : undefined),
      // TODO: extend with CDP-specific properties with `goog:` prefix after
      //  https://github.com/w3c/webdriver-bidi/pull/637
      //  * session: boolean;
      //  * priority: CookiePriority;
      //  * sameParty: boolean;
      //  * sourceScheme: CookieSourceScheme;
      //  * sourcePort: integer;
      //  * partitionKey?: string;
      //  * partitionKeyOpaque?: boolean;
    };
  }

  static #sameSiteCdpToBiDi(
    sameSite: Protocol.Network.CookieSameSite
  ): Network.SameSite {
    switch (sameSite) {
      case 'Strict':
        return Network.SameSite.Strict;
      case 'None':
        return Network.SameSite.None;
      case 'Lax':
        return Network.SameSite.Lax;
      default:
        // Defaults to `Lax`:
        // https://web.dev/articles/samesite-cookies-explained#samesitelax_by_default
        return Network.SameSite.Lax;
    }
  }

  static #sameSiteBiDiToCdp(
    sameSite: Network.SameSite
  ): Protocol.Network.CookieSameSite {
    switch (sameSite) {
      case Network.SameSite.Strict:
        return 'Strict';
      case Network.SameSite.Lax:
        return 'Lax';
      case Network.SameSite.None:
        return 'None';
    }
    throw new InvalidArgumentException(`Unknown 'sameSite' value ${sameSite}`);
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
