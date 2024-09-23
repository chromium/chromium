// Copyright 2015 The Crashpad Authors
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

#include "util/win/module_version.h"

#include <windows.h>
#include <stdint.h>

#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

namespace crashpad {

bool GetModuleVersionAndType(const base::FilePath& path,
                             VS_FIXEDFILEINFO* vs_fixedfileinfo) {
  DWORD size = GetFileVersionInfoSize(path.value().c_str(), nullptr);
  if (!size) {
    PLOG_IF(WARNING, GetLastError() != ERROR_RESOURCE_TYPE_NOT_FOUND)
        << "GetFileVersionInfoSize: " << base::WideToUTF8(path.value());
    return false;
  }

  auto data = base::HeapArray<uint8_t>::Uninit(size);
  if (!GetFileVersionInfo(path.value().c_str(),
                          0,
                          static_cast<DWORD>(data.size()),
                          data.data())) {
    PLOG(WARNING) << "GetFileVersionInfo: " << base::WideToUTF8(path.value());
    return false;
  }

  VS_FIXEDFILEINFO* fixed_file_info;
  UINT ffi_size;
  if (!VerQueryValue(data.data(),
                     L"\\",
                     reinterpret_cast<void**>(&fixed_file_info),
                     &ffi_size)) {
    PLOG(WARNING) << "VerQueryValue";
    return false;
  }

  *vs_fixedfileinfo = *fixed_file_info;
  vs_fixedfileinfo->dwFileFlags &= vs_fixedfileinfo->dwFileFlagsMask;
  return true;
}

}  // namespace crashpad
