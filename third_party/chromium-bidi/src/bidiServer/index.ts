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

import argparse from 'argparse';
import puppeteer, {PuppeteerNode} from 'puppeteer';

import mapperReader from './mapperReader.js';
import {MapperServer} from './mapperServer.js';
import {BidiServerRunner} from './bidiServerRunner.js';
import {ITransport} from '../utils/transport.js';

function parseArguments() {
  const parser = new argparse.ArgumentParser({
    add_help: true,
    exit_on_error: false,
  });

  parser.add_argument('-p', '--port', {
    help: 'Port that BiDi server should listen to. Default is 8080.',
    type: 'int',
    default: process.env['PORT'] || 8080,
  });

  parser.add_argument('-c', '--channel', {
    help:
      'If set, the given installed Chrome Release Channel will be used ' +
      'instead of one pointed by Puppeteer version. Can be one of ``, ' +
      '`chrome`, `chrome-beta`, `chrome-canary`, `chrome-dev`. The given ' +
      'Chrome channel should be installed. Default is ``.',
    default: process.env['CHANNEL'] || '',
  });

  parser.add_argument('-hl', '--headless', {
    help:
      'Sets if browser should run in headless or headful mode. Default is ' +
      '`--headless=true`.',
    default: true,
  });

  // `parse_known_args` puts known args in the first element of the result.
  const args = parser.parse_known_args();
  return args[0];
}

(() => {
  try {
    console.log('Launching BiDi server.');

    const args = parseArguments();
    const bidiPort = args.port;
    const headless = args.headless !== 'false';
    const chromeChannel = args.channel;

    new BidiServerRunner().run(bidiPort, (bidiServer) => {
      return _onNewBidiConnectionOpen(headless, chromeChannel, bidiServer);
    });
    console.log('BiDi server launched.');
  } catch (e) {
    console.log('Error', e);
  }
})();

/**
 * On each new BiDi connection:
 * 1. Launch Chromium (using Puppeteer for now).
 * 2. Get `BiDi-CDP` mapper JS binaries using `mapperReader`.
 * 3. Run `BiDi-CDP` mapper in launched browser.
 * 4. Bind `BiDi-CDP` mapper to the `BiDi server`.
 *
 * @returns delegate to be called when the connection is closed
 */
async function _onNewBidiConnectionOpen(
  headless: boolean,
  chromeChannel: string,
  bidiTransport: ITransport
): Promise<() => void> {
  const browserLaunchOptions: any = {
    headless,
  };
  if (chromeChannel) {
    browserLaunchOptions.channel = chromeChannel;
  }

  // 1. Launch Chromium (using Puppeteer for now).
  // Puppeteer should have downloaded Chromium during the installation.
  // Use Puppeteer's logic of launching browser as well.
  const browser = await (puppeteer as any as PuppeteerNode).launch(
    browserLaunchOptions
  );

  // No need in Puppeteer being connected to browser.
  browser.disconnect();

  // 2. Get `BiDi-CDP` mapper JS binaries using `mapperReader`.
  const bidiMapperScript = await mapperReader();

  // 3. Run `BiDi-CDP` mapper in launched browser.
  const mapperServer = await MapperServer.create(
    browser.wsEndpoint(),
    bidiMapperScript
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
    // Client disconnected. Close browser.
    await browser.close();
  };
}
