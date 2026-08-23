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

import {parseArgs} from 'node:util';

import {debugInfo, WebSocketServer} from './WebSocketServer.js';

export function parseCommandLineArgs() {
  const {values} = parseArgs({
    args: process.argv.slice(2),
    options: {
      port: {
        type: 'string',
        short: 'p',
        default: String(process.env['PORT'] ?? 8080),
      },
      verbose: {
        type: 'boolean',
        short: 'v',
        default: process.env['VERBOSE'] === 'true',
      },
    },
    strict: false,
  });

  return {
    port: Number(values.port),
    verbose: Boolean(values.verbose),
  };
}

try {
  const argv = parseCommandLineArgs();

  const {port, verbose} = argv;

  debugInfo('Launching BiDi server...');

  new WebSocketServer(port, verbose);
  debugInfo('BiDi server launched');
} catch (e) {
  debugInfo('Error launching BiDi server', e);
}
