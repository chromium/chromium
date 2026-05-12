// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#ifndef COMMON_H_
#define COMMON_H_

#include <cstdint>
#include <iostream>

#include "absl/log/absl_check.h"
#include "absl/log/globals.h"
#include "absl/log/absl_log.h"
#include "absl/strings/string_view.h"

#if defined(_WIN32) && !defined(__CYGWIN__)
#define OS_WIN
#else
#define OS_UNIX
#endif

#ifdef OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using char32 = uint32_t;

static constexpr uint32_t kUnicodeError = 0xFFFD;

#define FRIEND_TEST(a, b) friend class a##_Test_##b;

#define RETURN_IF_ERROR(expr)          \
  do {                                 \
    const auto _status = expr;         \
    if (!_status.ok()) return _status; \
  } while (0)

#endif  // COMMON_H_
