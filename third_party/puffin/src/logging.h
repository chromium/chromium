// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_LOGGING_H_
#define SRC_LOGGING_H_

#if defined(BASE_VER) && BASE_VER >= 822064
#include "base/check.h"  // CHECK-related macros are defined in base/check.h on Chrome OS.
#include "base/logging.h"
#elif USE_BRILLO
#include "base/logging.h"
#else
#include "glog/logging.h"
#endif

#define TEST_AND_RETURN_FALSE(_x)   \
  do {                              \
    if (!(_x)) {                    \
      LOG(ERROR) << #_x " failed."; \
      return false;                 \
    }                               \
  } while (0)

#define TEST_AND_RETURN_VALUE(_x, _v) \
  do {                                \
    if (!(_x)) {                      \
      LOG(ERROR) << #_x " failed.";   \
      return (_v);                    \
    }                                 \
  } while (0)

#endif  // SRC_LOGGING_H_
