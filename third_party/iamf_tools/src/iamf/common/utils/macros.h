/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef COMMON_UTILS_MACROS_H_
#define COMMON_UTILS_MACROS_H_

#include "absl/status/status_macros.h"

namespace iamf_tools {

// For propagating errors when calling a function.
#define RETURN_IF_NOT_OK(...) ABSL_RETURN_IF_ERROR((__VA_ARGS__))

// For propagating errors when calling a function, but ignoring errors when
// built with `-DIGNORE_ERRORS_USE_ONLY_FOR_IAMF_TEST_SUITE`. Beware that
// defining `IGNORE_ERRORS_USE_ONLY_FOR_IAMF_TEST_SUITE` is not thoroughly
// tested and may result in unexpected behavior. This define should only be used
// when creating test files which intentionally violate the IAMF spec.
#ifdef IGNORE_ERRORS_USE_ONLY_FOR_IAMF_TEST_SUITE
#define MAYBE_RETURN_IF_NOT_OK(...) \
  do {                              \
    (__VA_ARGS__).IgnoreError();    \
  } while (0)
#else
#define MAYBE_RETURN_IF_NOT_OK(...) ABSL_RETURN_IF_ERROR(__VA_ARGS__)
#endif

}  // namespace iamf_tools

#endif  // COMMON_UTILS_MACROS_H_
