// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

// onnxruntime::narrow() is like gsl::narrow() but it is also available when exceptions are disabled.

#if !defined(ORT_NO_EXCEPTIONS)

#include "gsl/narrow"

namespace onnxruntime {
using gsl::narrow;
}  // namespace onnxruntime

#else  // ^^ !defined(ORT_NO_EXCEPTIONS) ^^ / vv defined(ORT_NO_EXCEPTIONS) vv

#include <cstdio>     // std::fprintf
#include <exception>  // std::terminate
#include <type_traits>

#include "gsl/util"  // gsl::narrow_cast

namespace onnxruntime {

namespace detail {
[[noreturn]] inline void OnNarrowingError() noexcept {
  std::fprintf(stderr, "%s", "narrowing error\n");
  std::terminate();
}
}  // namespace detail

// This implementation of onnxruntime::narrow was copied and adapted from:
// https://github.com/microsoft/GSL/blob/a3534567187d2edc428efd3f13466ff75fe5805c/include/gsl/narrow

// narrow() : a checked version of narrow_cast() that terminates if the cast changed the value
template <class T, class U, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
GSL_SUPPRESS(type.1) constexpr T narrow(U u) noexcept {
  constexpr const bool is_different_signedness =
      (std::is_signed<T>::value != std::is_signed<U>::value);

  GSL_SUPPRESS(es.103)                 // don't overflow
  GSL_SUPPRESS(es.104)                 // don't underflow
  GSL_SUPPRESS(p.2)                    // don't rely on undefined behavior
  const T t = gsl::narrow_cast<T>(u);  // While this is technically undefined behavior in some cases (i.e., if the source value is of floating-point type
                                       // and cannot fit into the destination integral type), the resultant behavior is benign on the platforms
                                       // that we target (i.e., no hardware trap representations are hit).

  if (static_cast<U>(t) != u || (is_different_signedness && ((t < T{}) != (u < U{})))) {
    detail::OnNarrowingError();
  }

  return t;
}

template <class T, class U, typename std::enable_if<!std::is_arithmetic<T>::value>::type* = nullptr>
GSL_SUPPRESS(type.1) constexpr T narrow(U u) noexcept {
  const T t = gsl::narrow_cast<T>(u);

  if (static_cast<U>(t) != u) {
    detail::OnNarrowingError();
  }

  return t;
}

}  // namespace onnxruntime

#endif  // defined(ORT_NO_EXCEPTIONS)
