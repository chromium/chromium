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
 *
 */

/**
 * @fileoverview Utility functions for the Network domain.
 */
import type {Protocol} from 'devtools-protocol';

import type {Network} from '../../../protocol/protocol.js';

export function computeHeadersSize(headers: Network.Header[]): number {
  const requestHeaders = headers.reduce((acc, header) => {
    return `${acc}${header.name}: ${header.value.value}\r\n`;
  }, '');

  return new TextEncoder().encode(requestHeaders).length;
}

/** Converts from CDP Network domain headers to Bidi network headers. */
export function bidiNetworkHeadersFromCdpNetworkHeaders(
  headers?: Protocol.Network.Headers
): Network.Header[] {
  if (!headers) {
    return [];
  }

  return Object.entries(headers).map(([name, value]) => ({
    name,
    value: {
      type: 'string',
      value,
    },
  }));
}

/** Converts from Bidi network headers to CDP Network domain headers. */
export function cdpNetworkHeadersFromBidiNetworkHeaders(
  headers?: Network.Header[]
): Protocol.Network.Headers | undefined {
  if (headers === undefined) {
    return undefined;
  }

  return headers.reduce((result, header) => {
    // TODO: Distinguish between string and bytes?
    result[header.name] = header.value.value;
    return result;
  }, {} as Protocol.Network.Headers);
}

/** Converts from CDP Fetch domain header entries to Bidi network headers. */
export function bidiNetworkHeadersFromCdpFetchHeaders(
  headers?: Protocol.Fetch.HeaderEntry[]
): Network.Header[] {
  if (!headers) {
    return [];
  }

  return headers.map(({name, value}) => ({
    name,
    value: {
      type: 'string',
      value,
    },
  }));
}

/** Converts from Bidi network headers to CDP Fetch domain header entries. */
export function cdpFetchHeadersFromBidiNetworkHeaders(
  headers?: Network.Header[]
): Protocol.Fetch.HeaderEntry[] | undefined {
  if (headers === undefined) {
    return undefined;
  }

  return headers.map(({name, value}) => ({
    name,
    value: value.value,
  }));
}

/** Converts from Bidi auth action to CDP auth challenge response. */
export function cdpAuthChallengeResponseFromBidiAuthContinueWithAuthAction(
  action: 'default' | 'cancel' | 'provideCredentials'
) {
  switch (action) {
    case 'default':
      return 'Default';
    case 'cancel':
      return 'CancelAuth';
    case 'provideCredentials':
      return 'ProvideCredentials';
  }
}
