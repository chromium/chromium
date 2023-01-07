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

#include <string>

#include <lib/zx/thread.h>
#include <zircon/syscalls/object.h>
#include <zircon/types.h>

#include "base/check_op.h"
#include "base/fuchsia/fuchsia_logging.h"

namespace crashpad {
namespace test {

namespace {

std::string GetCurrentThreadName() {
  std::string result(ZX_MAX_NAME_LEN, '\0');
  const zx_status_t status = zx::thread::self()->get_property(
      ZX_PROP_NAME, result.data(), result.length());
  ZX_CHECK(status == ZX_OK, status) << "get_property(ZX_PROP_NAME)";
  const auto result_nul_idx = result.find('\0');
  CHECK_NE(result_nul_idx, std::string::npos)
      << "get_property() did not NUL terminate";
  result.resize(result_nul_idx);
  return result;
}

}  // namespace

ScopedSetThreadName::ScopedSetThreadName(const std::string& new_thread_name)
    : original_name_(GetCurrentThreadName()) {
  // Fuchsia silently truncates the thread name if it's too long.
  CHECK_LT(new_thread_name.length(), ZX_MAX_NAME_LEN);
  const zx_status_t status = zx::thread::self()->set_property(
      ZX_PROP_NAME, new_thread_name.c_str(), new_thread_name.length());
  ZX_CHECK(status == ZX_OK, status) << "set_property(ZX_PROP_NAME)";
}

ScopedSetThreadName::~ScopedSetThreadName() {
  const zx_status_t status = zx::thread::self()->set_property(
      ZX_PROP_NAME, original_name_.c_str(), original_name_.length());
  ZX_CHECK(status == ZX_OK, status) << "set_property(ZX_PROP_NAME)";
}

}  // namespace test
}  // namespace crashpad
