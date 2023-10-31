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

import {mkdtemp} from 'fs/promises';
import os from 'os';
import path from 'path';

import {
  Browser,
  CDP_WEBSOCKET_ENDPOINT_REGEX,
  type ChromeReleaseChannel,
  computeSystemExecutablePath,
  launch,
  type Process,
} from '@puppeteer/browsers';
import debug from 'debug';
import WebSocket from 'ws';

import {CdpConnection} from '../cdp/CdpConnection.js';
import {WebSocketTransport} from '../utils/WebsocketTransport.js';

import {MapperCdpConnection} from './MapperCdpConnection.js';
import {getMapperTabSource} from './reader.js';
import type {SimpleTransport} from './SimpleTransport.js';

const debugInternal = debug('bidi:mapper:internal');

/**
 * BrowserProcess is responsible for running the browser and BiDi Mapper within
 * it.
 * 1. Launch Chromium (using Puppeteer for now).
 * 2. Get `BiDi-CDP` mapper JS binaries using `MapperReader`.
 * 3. Run `BiDi-CDP` mapper in launched browser using `MapperRunner`.
 * 4. Bind `BiDi-CDP` mapper to the `BiDi server` to forward messages from BiDi
 * Mapper to the client.
 */
export class BrowserInstance {
  #mapperCdpConnection: MapperCdpConnection;
  #browserProcess: Process;

  static async run(
    channel: ChromeReleaseChannel,
    headless: boolean,
    verbose: boolean,
    chromeArgs?: string[]
  ): Promise<BrowserInstance> {
    const profileDir = await mkdtemp(
      path.join(os.tmpdir(), 'web-driver-bidi-server-')
    );
    // See https://github.com/GoogleChrome/chrome-launcher/blob/main/docs/chrome-flags-for-tools.md
    const chromeArguments = [
      ...(headless ? ['--headless', '--hide-scrollbars', '--mute-audio'] : []),
      // keep-sorted start
      '--allow-browser-signin=false',
      '--disable-component-update',
      '--disable-default-apps',
      '--disable-features=DialMediaRouteProvider',
      '--disable-notifications',
      '--disable-popup-blocking',
      '--enable-automation',
      '--no-default-browser-check',
      '--no-first-run',
      '--password-store=basic',
      '--remote-debugging-port=9222',
      '--use-mock-keychain',
      `--user-data-dir=${profileDir}`,
      // keep-sorted end
      ...(chromeArgs
        ? chromeArgs.filter((arg) => !arg.startsWith('--headless'))
        : []),
      'about:blank',
    ];

    const executablePath =
      process.env['BROWSER_BIN'] ??
      computeSystemExecutablePath({
        browser: Browser.CHROME,
        channel,
      });

    if (!executablePath) {
      throw new Error('Could not find Chrome binary');
    }

    const browserProcess = launch({
      executablePath,
      args: chromeArguments,
    });

    const cdpEndpoint = await browserProcess.waitForLineOutput(
      CDP_WEBSOCKET_ENDPOINT_REGEX
    );

    // There is a conflict between prettier and eslint here.
    // prettier-ignore
    const cdpConnection = await this.#establishCdpConnection(
      cdpEndpoint
    );

    // 2. Get `BiDi-CDP` mapper JS binaries.
    const mapperTabSource = await getMapperTabSource();

    // 3. Run `BiDi-CDP` mapper in launched browser using `MapperRunner`.
    const mapperCdpConnection = await MapperCdpConnection.create(
      cdpConnection,
      mapperTabSource,
      verbose
    );

    return new BrowserInstance(mapperCdpConnection, browserProcess);
  }

  constructor(
    mapperCdpConnection: MapperCdpConnection,
    browserProcess: Process
  ) {
    this.#mapperCdpConnection = mapperCdpConnection;
    this.#browserProcess = browserProcess;
  }

  async close() {
    // Close the mapper tab.
    this.#mapperCdpConnection.close();

    // Close browser.
    await this.#browserProcess.close();
  }

  bidiSession(): SimpleTransport {
    return this.#mapperCdpConnection.bidiSession();
  }

  static #establishCdpConnection(cdpUrl: string): Promise<CdpConnection> {
    return new Promise((resolve, reject) => {
      debugInternal('Establishing session with cdpUrl: ', cdpUrl);

      const ws = new WebSocket(cdpUrl);

      ws.once('error', reject);

      ws.on('open', () => {
        debugInternal('Session established.');

        const transport = new WebSocketTransport(ws);
        const connection = new CdpConnection(transport);
        resolve(connection);
      });
    });
  }
}
