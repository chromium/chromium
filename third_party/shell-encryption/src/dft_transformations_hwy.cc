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

#include "dft_transformations_hwy.h"

#include <cmath>
#include <utility>
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "dft_transformations.h"
#include "montgomery.h"
#include "ntt_parameters.h"
#include "status_macros.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "dft_transformations_hwy.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/print-inl.h"

HWY_BEFORE_NAMESPACE();
namespace rlwe::internal {
namespace HWY_NAMESPACE {

#if HWY_TARGET == HWY_SCALAR

template <typename ModularInt>
void IterativeCooleyTukeyHwy(std::vector<ModularInt>& coeffs, int log_len,
                             const NttParameters<ModularInt>& ntt_params,
                             const typename ModularInt::Params& mod_params) {
  return IterativeCooleyTukey<ModularInt>(coeffs, log_len, ntt_params,
                                          mod_params);
}

#else

namespace hn = hwy::HWY_NAMESPACE;

// Reduces x in range [0, 2 * q) to [0, q).
template <typename Int>
hn::Vec<hn::ScalableTag<Int>> SimpleModHwy(hn::Vec<hn::ScalableTag<Int>> x,
                                           hn::Vec<hn::ScalableTag<Int>> q) {
  const hn::ScalableTag<Int> d;
  hn::Mask<decltype(d)> mask = hn::Ge(x, q);
  hn::Vec<decltype(d)> res = hn::Sub(x, hn::IfThenElseZero(mask, q));
  return res;
}

// Iterative Cooley-Tukey implemented using go/hwy.
template <typename ModularInt>
void IterativeCooleyTukeyHwy(std::vector<ModularInt>& coeffs, int log_len,
                             const NttParameters<ModularInt>& ntt_params,
                             const typename ModularInt::Params& mod_params) {
  const hn::ScalableTag<typename ModularInt::Int> d;
  const int num_lanes = hn::Lanes(d);
  const int log_num_lanes = std::ceil(std::log2(num_lanes));

  using Vec = hn::Vec<decltype(d)>;

  const Vec neg_modulus = hn::Set(d, -mod_params.modulus);
  const Vec twice_modulus = hn::Set(d, 2 * mod_params.modulus);
  int index_psi = 1;
  using Int = typename ModularInt::Int;

  // Vectorized implementation for the first serveral layers.
  int i = log_len - 1;
  for (; i >= log_num_lanes; i--) {
    // Layer i.
    const int half_m = 1 << i;
    const int m = half_m << 1;

    for (int k = 0; k < coeffs.size(); k += m) {
      const auto& [psi_constant_scalar, psi_constant_barrett_scalar] =
          ntt_params.psis_bitrev_constant[index_psi];
      Vec psi_constant = hn::Set(d, psi_constant_scalar);
      Vec psi_constant_barrett = hn::Set(d, psi_constant_barrett_scalar);
      // Vectorized Cooley-Tukey butterfly.
      // Invariant: coefficients are in range [0, 4 * modulus) after the loop
      // terminates (except for the last layer - see below).
      for (int j = 0; j < half_m; j += num_lanes) {
        Int* ptr = reinterpret_cast<Int*>(&coeffs[k + j]);
        Vec v = hn::LoadU(d, ptr + half_m);

        Vec q = hn::MulHigh(psi_constant_barrett, v);
        Vec psi_v_prod = hn::Mul(psi_constant, v);
        // t in range [0, 2 * modulus)
        Vec t = hn::Add(psi_v_prod, hn::Mul(q, neg_modulus));

        Vec u = hn::LoadU(d, ptr);
        // Reduce u to [0, 2 * modulus)
        u = SimpleModHwy<Int>(u, twice_modulus);

        // u, v both in range [0, 4 * modulus)
        v = hn::Add(u, hn::Sub(twice_modulus, t));
        u = hn::Add(u, t);

        // For the last layer we reduce it mod 2 * modulus. Technically this
        // is only required for word size - 2 bit modulus because of the
        // overflow in the scalar implementation, but we just handle it
        // generally for now).
        if (i == log_num_lanes) {
          u = SimpleModHwy<Int>(u, twice_modulus);
          v = SimpleModHwy<Int>(v, twice_modulus);
        }

        hn::StoreU(u, d, ptr);
        hn::StoreU(v, d, ptr + half_m);
      }
      index_psi++;
    }
  }

  // Scalar implementation for the remaining layers.
  bool should_lazy_add = false;
  for (; i >= 0; i--) {
    // Layer i.
    const int half_m = 1 << i;
    const int m = half_m << 1;

    for (int k = 0; k < coeffs.size(); k += m) {
      const auto& [psi_constant_scalar, psi_constant_barrett_scalar] =
          ntt_params.psis_bitrev_constant[index_psi];
      for (int j = 0; j < half_m; j++) {
        // Scalar Cooley-Tukey butterfly operation.
        ModularInt t = coeffs[k + j + half_m].MulConstant(
            psi_constant_scalar, psi_constant_barrett_scalar, &mod_params);
        ModularInt u = coeffs[k + j];
        if (i != 0 && should_lazy_add) {
          coeffs[k + j].LazyAddInPlace(t, &mod_params);
          // Note that the variable t is reduced in [0, modulus], which is
          // required by the LazySubInPlace function.
          coeffs[k + j + half_m] = std::move(u.LazySubInPlace(t, &mod_params));
        } else {
          coeffs[k + j].AddInPlace(t, &mod_params);
          // Note that the variable t is reduced in [0, modulus], which is
          // required by the SubInPlace function.
          coeffs[k + j + half_m] = std::move(u.SubInPlace(t, &mod_params));
        }
      }
      index_psi++;
    }
    should_lazy_add = !should_lazy_add;
  }
}

#endif

}  // namespace HWY_NAMESPACE
}  // namespace rlwe::internal
HWY_AFTER_NAMESPACE();

#if HWY_ONCE || HWY_IDE
namespace rlwe::internal {

template <typename ModularInt>
absl::Status ForwardNumberTheoreticTransformHwy(
    std::vector<ModularInt>& coeffs,
    const NttParameters<ModularInt>& ntt_params,
    const typename ModularInt::Params& mod_params) {
  int log_len = log2(coeffs.size());
  int expected_len = 1 << log_len;
  if (coeffs.size() != expected_len) {
    return absl::InvalidArgumentError(absl::StrCat(
        "`coeffs` must have contain ", expected_len, " elements."));
  }

  HWY_EXPORT_AND_DYNAMIC_DISPATCH_T(IterativeCooleyTukeyHwy<ModularInt>)
  (coeffs, log_len, ntt_params, mod_params);
  return absl::OkStatus();
}

template <>
absl::Status ForwardNumberTheoreticTransformHwy(
    std::vector<MontgomeryInt<absl::uint128>>& coeffs,
    const NttParameters<MontgomeryInt<absl::uint128>>& ntt_params,
    const typename MontgomeryInt<absl::uint128>::Params& mod_params) {
  return ForwardNumberTheoreticTransform<MontgomeryInt<absl::uint128>>(
      coeffs, ntt_params, mod_params);
}

#ifdef ABSL_HAVE_INTRINSIC_INT128

template <>
absl::Status ForwardNumberTheoreticTransformHwy(
    std::vector<MontgomeryInt<unsigned __int128>>& coeffs,
    const NttParameters<MontgomeryInt<unsigned __int128>>& ntt_params,
    const typename MontgomeryInt<unsigned __int128>::Params& mod_params) {
  return ForwardNumberTheoreticTransform<MontgomeryInt<unsigned __int128>>(
      coeffs, ntt_params, mod_params);
}

#endif

// If using other integer type, need to do something similar as below to avoid
// link error.
template absl::Status ForwardNumberTheoreticTransformHwy(
    std::vector<MontgomeryInt<uint16_t>>& coeffs,
    const NttParameters<MontgomeryInt<uint16_t>>& ntt_params,
    const typename MontgomeryInt<uint16_t>::Params& mod_params);

template absl::Status ForwardNumberTheoreticTransformHwy(
    std::vector<MontgomeryInt<uint32_t>>& coeffs,
    const NttParameters<MontgomeryInt<uint32_t>>& ntt_params,
    const typename MontgomeryInt<uint32_t>::Params& mod_params);

template absl::Status ForwardNumberTheoreticTransformHwy(
    std::vector<MontgomeryInt<uint64_t>>& coeffs,
    const NttParameters<MontgomeryInt<uint64_t>>& ntt_params,
    const typename MontgomeryInt<uint64_t>::Params& mod_params);

}  // namespace rlwe::internal

#endif