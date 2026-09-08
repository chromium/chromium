/**
 * Copyright 2024 Google LLC.
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

/**
 * @fileoverview `node http-proxy.mjs [host]` starts a mock HTTP proxy server on
 * {host}:{FREE_PORT} (defaulting to localhost). Upon startup, it prints
 * `Listening on {host}:{port}` to stdout. For each incoming request, it logs
 * the requested URL to stdout and returns a predefined 200 OK HTML response
 * without forwarding the request.
 */

import http from 'node:http';

function log(...args) {
  console.log(...args);
}

const proxyServer = http
  .createServer((originalRequest, originalResponse) => {
    log(originalRequest.url);
    originalResponse.writeHead(200, {
      'Content-Type': 'text/html; charset=UTF-8',
    });
    originalResponse.end('<html><body>Proxied response</body></html>');
  })
  .listen(() => {
    log(
      `Listening on ${process.argv[2] || 'localhost'}:${proxyServer.address().port}`,
    );
  });
