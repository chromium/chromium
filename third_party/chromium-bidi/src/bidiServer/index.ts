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

import {ChromeReleaseChannel} from '@puppeteer/browsers';
import {ArgumentParser} from 'argparse';

import {debugInfo, WebSocketServer} from './WebSocketServer.js';

function parseArguments(): {
  channel: ChromeReleaseChannel;
  port: number;
  verbose: boolean;
} {
  const parser = new ArgumentParser({
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
    const verbose = args.verbose === true;

    debugInfo('Launching BiDi server...');

    new WebSocketServer(port, channel, verbose);
    debugInfo('BiDi server launched');
  } catch (e) {
    debugInfo('Error launching BiDi server', e);
  }
})();
