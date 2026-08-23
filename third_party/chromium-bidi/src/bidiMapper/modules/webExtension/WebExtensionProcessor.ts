/**
 * Copyright 2025 Google LLC.
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
import {
  InvalidWebExtensionException,
  type WebExtension,
  UnsupportedOperationException,
  type EmptyResult,
  NoSuchWebExtensionException,
} from '../../../protocol/protocol.js';

/**
 * Responsible for handling the `webModule` module.
 */
export class WebExtensionProcessor {
  readonly #browserCdpClient: CdpClient;

  constructor(browserCdpClient: CdpClient) {
    this.#browserCdpClient = browserCdpClient;
  }

  async install(
    params: WebExtension.InstallParameters,
  ): Promise<WebExtension.InstallResult> {
    switch (params.extensionData.type) {
      case 'archivePath':
      case 'base64':
        throw new UnsupportedOperationException(
          'Archived and Base64 extensions are not supported',
        );
      case 'path':
        break;
    }
    try {
      const response = await this.#browserCdpClient.sendCommand(
        'Extensions.loadUnpacked',
        {
          path: params.extensionData.path,
        },
      );
      return {
        extension: response.id,
      };
    } catch (err) {
      if ((err as Error).message.startsWith('invalid web extension')) {
        throw new InvalidWebExtensionException((err as Error).message);
      }
      throw err;
    }
  }

  async uninstall(
    params: WebExtension.UninstallParameters,
  ): Promise<EmptyResult> {
    try {
      await this.#browserCdpClient.sendCommand('Extensions.uninstall', {
        id: params.extension,
      });
      return {};
    } catch (err) {
      if (
        (err as Error).message ===
        'Uninstall failed. Reason: could not find extension.'
      ) {
        throw new NoSuchWebExtensionException('no such web extension');
      }
      throw err;
    }
  }
}
