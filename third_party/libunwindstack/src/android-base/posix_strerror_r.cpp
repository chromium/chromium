//
// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

/* Undefine _GNU_SOURCE so that this compilation unit can access the
 * posix version of strerror_r */
#undef _GNU_SOURCE
#include <string.h>

namespace android {
namespace base {

extern "C" int posix_strerror_r(int errnum, char* buf, size_t buflen) {
#ifdef _WIN32
  return strerror_s(buf, buflen, errnum);
#else
  return strerror_r(errnum, buf, buflen);
#endif
}

}  // namespace base
}  // namespace android
