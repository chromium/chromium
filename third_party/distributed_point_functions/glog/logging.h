// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_DISTRIBUTED_POINT_FUNCTIONS_GLOG_LOGGING_H_
#define THIRD_PARTY_DISTRIBUTED_POINT_FUNCTIONS_GLOG_LOGGING_H_

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"

// Manually define function missing from base
#define LOG_FIRST_N(severity, n)  \
  static int LOG_OCCURRENCES = 0; \
  if (LOG_OCCURRENCES <= n)       \
    ++LOG_OCCURRENCES;            \
  if (LOG_OCCURRENCES <= n)       \
  LOG(severity)

#endif  // THIRD_PARTY_DISTRIBUTED_POINT_FUNCTIONS_GLOG_LOGGING_H_
