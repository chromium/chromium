// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "util/fuchsia/koid_utilities.h"

#include <lib/fdio/fdio.h>
#include <lib/zx/channel.h>
#include <lib/zx/job.h>
#include <lib/zx/process.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "util/file/file_io.h"

namespace crashpad {

namespace {

// Casts |handle| into a container of type T, returning a null handle if the
// actual handle type does not match that of T.
template <typename T>
T CastHandle(zx::handle handle) {
  zx_info_handle_basic_t actual = {};
  zx_status_t status = handle.get_info(
      ZX_INFO_HANDLE_BASIC, &actual, sizeof(actual), nullptr, nullptr);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_object_get_info";
    return T();
  }
  if (actual.type != T::TYPE) {
    LOG(ERROR) << "Wrong type: " << actual.type << ", expected " << T::TYPE;
    return T();
  }
  return T(std::move(handle));
}

// Returns null handle if |koid| is not found or an error occurs. If |was_found|
// is non-null then it will be set, to distinguish not-found from other errors.
template <typename T, typename U>
T GetChildHandleByKoid(const U& parent, zx_koid_t child_koid, bool* was_found) {
  zx::handle handle;
  zx_status_t status =
      parent.get_child(child_koid, ZX_RIGHT_SAME_RIGHTS, &handle);
  if (was_found)
    *was_found = (status != ZX_ERR_NOT_FOUND);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_object_get_child";
    return T();
  }

  return CastHandle<T>(std::move(handle));
}

}  // namespace

std::vector<zx_koid_t> GetChildKoids(const zx::object_base& parent_object,
                                     zx_object_info_topic_t child_kind) {
  size_t actual = 0;
  size_t available = 0;
  std::vector<zx_koid_t> result(100);
  zx::unowned_handle parent(parent_object.get());

  // This is inherently racy. Better if the process is suspended, but there's
  // still no guarantee that a thread isn't externally created. As a result,
  // must be in a retry loop.
  for (;;) {
    zx_status_t status = parent->get_info(child_kind,
                                          result.data(),
                                          result.size() * sizeof(zx_koid_t),
                                          &actual,
                                          &available);
    // If the buffer is too small (even zero), the result is still ZX_OK, not
    // ZX_ERR_BUFFER_TOO_SMALL.
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_object_get_info";
      break;
    }

    if (actual == available) {
      break;
    }

    // Resize to the expected number next time, with a bit of slop to handle the
    // race between here and the next request.
    result.resize(available + 10);
  }

  result.resize(actual);
  return result;
}

std::vector<zx::thread> GetThreadHandles(const zx::process& parent) {
  auto koids = GetChildKoids(parent, ZX_INFO_PROCESS_THREADS);
  return GetHandlesForThreadKoids(parent, koids);
}

std::vector<zx::thread> GetHandlesForThreadKoids(
    const zx::process& parent,
    const std::vector<zx_koid_t>& koids) {
  std::vector<zx::thread> result;
  result.reserve(koids.size());
  for (zx_koid_t koid : koids) {
    result.emplace_back(GetThreadHandleByKoid(parent, koid));
  }
  return result;
}

zx::thread GetThreadHandleByKoid(const zx::process& parent,
                                 zx_koid_t child_koid) {
  return GetChildHandleByKoid<zx::thread>(parent, child_koid, nullptr);
}

zx_koid_t GetKoidForHandle(const zx::object_base& object) {
  zx_info_handle_basic_t info;
  zx_status_t status = zx_object_get_info(object.get(),
                                          ZX_INFO_HANDLE_BASIC,
                                          &info,
                                          sizeof(info),
                                          nullptr,
                                          nullptr);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_object_get_info";
    return ZX_KOID_INVALID;
  }
  return info.koid;
}

}  // namespace crashpad
