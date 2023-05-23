/**
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
import crypto from 'crypto';

/**
 * Generates a random v4 UUID, as specified in RFC4122.
 *
 * Uses the native Web Crypto API if available, otherwise falls back to a
 * polyfill.
 *
 * Example: '9b1deb4d-3b7d-4bad-9bdd-2b0d7b3dcb6d'
 */
export function uuidv4(): `${string}-${string}-${string}-${string}-${string}` {
  // Available only in secure contexts
  // https://developer.mozilla.org/en-US/docs/Web/API/Web_Crypto_API
  if (crypto.randomUUID) {
    return crypto.randomUUID();
  }

  const randomValues = new Uint8Array(16);
  crypto.getRandomValues(randomValues);

  // Set version (4) and variant (RFC4122) bits.
  randomValues[6] = (randomValues[6]! & 0x0f) | 0x40;
  randomValues[8] = (randomValues[8]! & 0x3f) | 0x80;

  const bytesToHex = (bytes: Uint8Array) =>
    bytes.reduce((str, byte) => str + byte.toString(16).padStart(2, '0'), '');

  return [
    bytesToHex(randomValues.subarray(0, 4)),
    bytesToHex(randomValues.subarray(4, 6)),
    bytesToHex(randomValues.subarray(6, 8)),
    bytesToHex(randomValues.subarray(8, 10)),
    bytesToHex(randomValues.subarray(10, 16)),
  ].join('-') as `${string}-${string}-${string}-${string}-${string}`;
}
