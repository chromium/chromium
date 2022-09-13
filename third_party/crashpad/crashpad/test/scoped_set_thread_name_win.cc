// Copyright 2022 The Crashpad Authors
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
#include "util/win/get_function.h"
#include "util/win/scoped_local_alloc.h"

namespace crashpad {
namespace test {

namespace {

auto GetThreadDescriptionFuncPtr() {
  static const auto get_thread_description =
      GET_FUNCTION(L"kernel32.dll", ::GetThreadDescription);
  return get_thread_description;
}

auto SetThreadDescriptionFuncPtr() {
  static const auto set_thread_description =
      GET_FUNCTION(L"kernel32.dll", ::SetThreadDescription);
  return set_thread_description;
}

std::wstring GetCurrentThreadName() {
  wchar_t* thread_description;
  const auto get_thread_description = GetThreadDescriptionFuncPtr();
  DCHECK(get_thread_description);
  HRESULT hr = get_thread_description(GetCurrentThread(), &thread_description);
  CHECK(SUCCEEDED(hr)) << "GetThreadDescription: "
                       << logging::SystemErrorCodeToString(hr);
  ScopedLocalAlloc thread_description_owner(thread_description);
  return std::wstring(thread_description);
}

void SetCurrentThreadName(const std::wstring& new_thread_name) {
  const auto set_thread_description = SetThreadDescriptionFuncPtr();
  DCHECK(set_thread_description);
  HRESULT hr =
      set_thread_description(GetCurrentThread(), new_thread_name.c_str());
  CHECK(SUCCEEDED(hr)) << "SetThreadDescription: "
                       << logging::SystemErrorCodeToString(hr);
}

}  // namespace

ScopedSetThreadName::ScopedSetThreadName(const std::string& new_thread_name)
    : original_name_() {
  if (IsSupported()) {
    original_name_.assign(GetCurrentThreadName());
    SetCurrentThreadName(base::UTF8ToWide(new_thread_name));
  }
}

ScopedSetThreadName::~ScopedSetThreadName() {
  if (IsSupported()) {
    SetCurrentThreadName(original_name_);
  }
}

// static
bool ScopedSetThreadName::IsSupported() {
  return GetThreadDescriptionFuncPtr() && SetThreadDescriptionFuncPtr();
}

}  // namespace test
}  // namespace crashpad
