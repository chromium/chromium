/**
 * Copyright 2021 Google LLC.
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

import path from 'path';
import os from 'os';
import {mkdtemp} from 'fs/promises';

import argparse from 'argparse';
import {
  launch,
  computeSystemExecutablePath,
  Browser,
  CDP_WEBSOCKET_ENDPOINT_REGEX,
  ChromeReleaseChannel,
} from '@puppeteer/browsers';

import type {ITransport} from '../utils/transport.js';

import {BidiServerRunner, debugInfo} from './bidiServerRunner.js';
import {MapperServer} from './mapperServer.js';
import mapperReader from './mapperReader.js';

function parseArguments(): {
  channel: ChromeReleaseChannel;
  headless: string;
  port: number;
  verbose: boolean;
} {
  const parser = new argparse.ArgumentParser({
    add_help: true,
    exit_on_error: true,
  });

  parser.add_argument('-c', '--channel', {
    help:
      'If set, the given installed Chrome Release Channel will be used ' +
      'instead of one pointed by Puppeteer version',
    choices: Object.values(ChromeReleaseChannel),
    default: ChromeReleaseChannel.DEV,
  });

  parser.add_argument('--headless', {
    help: 'Sets if browser should run in headless or headful mode. Default is true.',
    default: true,
  });

  parser.add_argument('-p', '--port', {
    help: 'Port that BiDi server should listen to. Default is 8080.',
    type: 'int',
    default: process.env['PORT'] ?? 8080,
  });

  parser.add_argument('-v', '--verbose', {
    help: 'If present, the Mapper debug log, including CDP commands and events will be logged into the server output.',
    action: 'store_true',
    default: process.env['VERBOSE'] === 'true' || false,
  });

  return parser.parse_known_args()[0];
}

(() => {
  try {
    const args = parseArguments();
    const {channel, port} = args;
    const headless = args.headless !== 'false';
    const verbose = args.verbose === true;

    debugInfo('Launching BiDi server...');

    new BidiServerRunner().run(port, (bidiServer, chromeArgs) => {
      return onNewBidiConnectionOpen(
        channel,
        headless,
        bidiServer,
        verbose,
        chromeArgs
      );
    });
    debugInfo('BiDi server launched');
  } catch (e) {
    debugInfo('Error', e);
  }
})();

/**
 * On each new BiDi connection:
 * 1. Launch Chromium (using Puppeteer for now).
 * 2. Get `BiDi-CDP` mapper JS binaries using `mapperReader`.
 * 3. Run `BiDi-CDP` mapper in launched browser.
 * 4. Bind `BiDi-CDP` mapper to the `BiDi server`.
 *
 * @return delegate to be called when the connection is closed
 */
async function onNewBidiConnectionOpen(
  channel: ChromeReleaseChannel,
  headless: boolean,
  bidiTransport: ITransport,
  verbose: boolean,
  chromeArgs?: string[]
) {
  // 1. Launch the browser using @puppeteer/browsers.
  const profileDir = await mkdtemp(
    path.join(os.tmpdir(), 'web-driver-bidi-server-')
  );
  // See https://github.com/GoogleChrome/chrome-launcher/blob/main/docs/chrome-flags-for-tools.md
  const chromeArguments = [
    ...(headless ? ['--headless', '--hide-scrollbars', '--mute-audio'] : []),
    // keep-sorted start
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

  const browser = launch({
    executablePath,
    args: chromeArguments,
  });

  const wsEndpoint = await browser.waitForLineOutput(
    CDP_WEBSOCKET_ENDPOINT_REGEX
  );

  // 2. Get `BiDi-CDP` mapper JS binaries using `mapperReader`.
  const bidiMapperScript = await mapperReader();

  // 3. Run `BiDi-CDP` mapper in launched browser.
  const mapperServer = await MapperServer.create(
    wsEndpoint,
    bidiMapperScript,
    verbose
  );

  // 4. Bind `BiDi-CDP` mapper to the `BiDi server`.
  // Forward messages from BiDi Mapper to the client.
  mapperServer.setOnMessage(async (message) => {
    await bidiTransport.sendMessage(message);
  });

  // Forward messages from the client to BiDi Mapper.
  bidiTransport.setOnMessage(async (message) => {
    await mapperServer.sendMessage(message);
  });

  // Return delegate to be called when the connection is closed.
  return async () => {
    // Close the mapper server.
    mapperServer.close();

    // Close browser.
    await browser.close();
  };
}
