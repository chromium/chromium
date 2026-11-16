/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "android-base/stringprintf.h"

#include <stdio.h>

#include <string>

namespace android {
namespace base {

void StringAppendV(std::string* dst, const char* format, va_list ap) {
  // First try with a small fixed size buffer
  char space[1024] __attribute__((__uninitialized__));

  // It's possible for methods that use a va_list to invalidate
  // the data in it upon use.  The fix is to make a copy
  // of the structure before using it and use that copy instead.
  va_list backup_ap;
  va_copy(backup_ap, ap);
  int n = vsnprintf(space, sizeof(space), format, backup_ap);
  va_end(backup_ap);

  if (n < static_cast<int>(sizeof(space))) {
    if (n >= 0) {
      // Normal case -- everything fit.
      dst->append(space, n);
      return;
    }

    if (n < 0) {
      // Just an error.
      return;
    }
  }

  // Since C++11 it's okay to write one past the end,
  // as long as you only write '\0'.
  // https://cplusplus.github.io/LWG/issue2475
  size_t old_size = dst->size();
  dst->resize(old_size + n);
  // Reset the va_list before we use it again.
  va_copy(backup_ap, ap);
  vsnprintf(dst->data() + old_size, n + 1, format, backup_ap);
  va_end(backup_ap);
}

std::string StringPrintf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::string result;
  StringAppendV(&result, fmt, ap);
  va_end(ap);
  return result;
}

void StringAppendF(std::string* dst, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  StringAppendV(dst, format, ap);
  va_end(ap);
}

}  // namespace base
}  // namespace android
