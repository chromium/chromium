/*
 * Copyright 2017 Google LLC.
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
#include "ntt_parameters.h"

namespace rlwe {
namespace internal {

// Bit reverse only among the rightmost log_n bytes.
unsigned int Bitrev(unsigned int input, unsigned int log_n) {
  unsigned int output = 0;
  for (unsigned int i = 0; i < log_n; i++) {
    output <<= 1;
    output |= input & 0x01;
    input >>= 1;
  }

  return output;
}

std::vector<unsigned int> BitrevArray(unsigned int log_n) {
  unsigned int n = 1 << log_n;
  std::vector<unsigned int> output(n);

  for (unsigned int i = 0; i < n; i++) {
    output[i] = Bitrev(i, log_n);
  }

  return output;
}

}  // namespace internal
}  // namespace rlwe
