/*
 * Copyright 2025 Google LLC.
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

#ifndef RLWE_DFT_TRANSFORMATIONS_HWY_H_
#define RLWE_DFT_TRANSFORMATIONS_HWY_H_

#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "ntt_parameters.h"
#include "status_macros.h"
#include "third_party/shell-encryption/base/shell_encryption_export.h"

namespace rlwe::internal {

// Performs the forward NTT transformation from R_q = Z[X]/(q, X^N+1) to Z_q^N
// in-place, where q is a prime and N is a power of two such q == 1 (mod 2N).
// The input polynomial is represented by its coefficient vector `coeffs`.
template <typename ModularInt>
extern SHELL_ENCRYPTION_EXPORT absl::Status ForwardNumberTheoreticTransformHwy(
    std::vector<ModularInt>& coeffs,
    const NttParameters<ModularInt>& ntt_params,
    const typename ModularInt::Params& mod_params);

}  // namespace rlwe::internal

#endif  // RLWE_DFT_TRANSFORMATIONS_HWY_H_
