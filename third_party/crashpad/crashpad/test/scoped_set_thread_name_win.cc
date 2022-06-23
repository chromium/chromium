// Copyright 2022 The Crashpad Authors. All rights reserved.
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
// limitations under the License.

#include "test/scoped_set_thread_name.h"

#include <windows.h>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "util/win/scoped_local_alloc.h"

namespace crashpad {
namespace test {

namespace {

std::wstring GetCurrentThreadName() {
  wchar_t* thread_description;
  HRESULT hr = GetThreadDescription(GetCurrentThread(), &thread_description);
  CHECK(SUCCEEDED(hr)) << "GetThreadDescription: "
                       << logging::SystemErrorCodeToString(hr);
  ScopedLocalAlloc thread_description_owner(thread_description);
  return std::wstring(thread_description);
}

}  // namespace

ScopedSetThreadName::ScopedSetThreadName(const std::string& new_thread_name)
    : original_name_(GetCurrentThreadName()) {
  const std::wstring wnew_thread_name = base::UTF8ToWide(new_thread_name);
  HRESULT hr =
      SetThreadDescription(GetCurrentThread(), wnew_thread_name.c_str());
  CHECK(SUCCEEDED(hr)) << "SetThreadDescription: "
                       << logging::SystemErrorCodeToString(hr);
}

ScopedSetThreadName::~ScopedSetThreadName() {
  HRESULT hr = SetThreadDescription(GetCurrentThread(), original_name_.c_str());
  CHECK(SUCCEEDED(hr)) << "SetThreadDescription: "
                       << logging::SystemErrorCodeToString(hr);
}

}  // namespace test
}  // namespace crashpad
