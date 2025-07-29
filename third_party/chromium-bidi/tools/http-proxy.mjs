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
 * @fileoverview `node http-proxy.mjs` starts a new HTTP server on
 * localhost:{FREE_PORT}. Upon the start the server prints its URL to stdout.
 * After that it will print a URL per line for each of the requests it proxied.
 * It does not forward requests, and always provides a predefined response.
 */

import http from 'node:http';

function log(...args) {
  console.log(...args);
}

const proxyServer = http
  .createServer((originalRequest, originalResponse) => {
    log(originalRequest.url);
    const proxyRequest = http.request(
      originalRequest.url,
      {
        method: originalRequest.method,
        headers: originalRequest.headers,
      },
      (proxyResponse) => {
        originalResponse.writeHead(
          proxyResponse.statusCode,
          proxyResponse.headers,
        );
        proxyResponse.pipe(originalResponse, {end: true});
      },
    );

    originalRequest.pipe(proxyRequest, {end: true});
  })
  .listen(() => {
    log(
      `Listening on ${process.argv[2] || 'localhost'}:${proxyServer.address().port}`,
    );
  });
