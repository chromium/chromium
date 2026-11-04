/* Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/portable/serialize.h"

uint8_t Fletcher8(const uint8_t* data, size_t size, uint8_t init) {
  uint_fast32_t sum1 = init & 0xf;
  uint_fast32_t sum2 = init >> 4;

  while (size > 0) {
    /* To reduce the number of % ops, a standard optimization is to accumulate
     * as much as possible before sum2 could overflow [due to Nakassis, 1988,
     * "Fletcher's error detection algorithm: how to implement it efficiently
     * and how to avoid the most common pitfalls"]. After n steps:
     *
     *   sum1 <= 14 + 255 n,
     *   sum2 <= 14 + 14 n + 255 (n + 1) n / 2.
     *
     * So sum2 <= 2^32 - 1 for n <= 5803.
     */
    const size_t kMaxBlockSize = 5803;
    const int block_size = (int)(size < kMaxBlockSize ? size : kMaxBlockSize);

    int i;
    for (i = 0; i < block_size; ++i) {
      sum1 += data[i];
      sum2 += sum1;
    }

    /* NOTE: There are strategies to compute modulo 15 or 255 without division.
     * But if the hardware has an integer divide instruction, the usual `%` is
     * often comparable or faster. See
     * http://homepage.divms.uiowa.edu/~jones/bcd/mod.shtml#exmod15
     */
    sum1 %= 15;
    sum2 %= 15;
    data += block_size;
    size -= block_size;
  }

  return (uint8_t)(sum2 << 4 | sum1);
}

uint16_t Fletcher16(const uint8_t* data, size_t size, uint16_t init) {
  uint_fast32_t sum1 = init & 0xff;
  uint_fast32_t sum2 = init >> 8;

  while (size > 0) {
    /* After n steps:
     *
     *   sum1 <= 254 + 255 n,
     *   sum2 <= 254 + 254 n + 255 (n + 1) n / 2.
     *
     * So sum2 <= 2^32 - 1 for n <= 5802.
     */
    const size_t kMaxBlockSize = 5802;
    const int block_size = (int)(size < kMaxBlockSize ? size : kMaxBlockSize);

    int i;
    for (i = 0; i < block_size; ++i) {
      sum1 += data[i];
      sum2 += sum1;
    }

    sum1 %= 255;
    sum2 %= 255;
    data += block_size;
    size -= block_size;
  }

  return (uint16_t)(sum2 << 8 | sum1);
}
