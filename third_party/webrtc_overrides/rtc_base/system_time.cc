/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>

#include "base/time/time.h"

namespace rtc {

int64_t SystemTimeNanos() {
  return (base::TimeTicks::Now() - base::TimeTicks()).InNanoseconds();
}

}  // namespace rtc
