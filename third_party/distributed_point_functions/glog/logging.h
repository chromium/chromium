// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_DISTRIBUTED_POINT_FUNCTIONS_GLOG_LOGGING_H_
#define THIRD_PARTY_DISTRIBUTED_POINT_FUNCTIONS_GLOG_LOGGING_H_

#include <stdlib.h>

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
#define DCHECK_IS_ON() false
#else
#define DCHECK_IS_ON() true
#endif

#if DCHECK_IS_ON()

#define DCHECK(condition)                                    \
  if (!(condition)) {                                        \
    printf("Check failed: function %s, file %s, line %d.\n", \
           __PRETTY_FUNCTION__, __FILE__, __LINE__);         \
    abort();                                                 \
  }

#else

#define DCHECK(condition) (void)(condition)

#endif

#endif  // THIRD_PARTY_DISTRIBUTED_POINT_FUNCTIONS_GLOG_LOGGING_H_
