// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_TEST_INTEGRATION_TEST_HELPERS_H_
#define UI_OZONE_PLATFORM_DRM_TEST_INTEGRATION_TEST_HELPERS_H_

#include <string>
#include <utility>

#include "base/files/scoped_file.h"

namespace base {
class FilePath;
}  // namespace base

namespace ui::test {

using PathAndFd = std::pair<base::FilePath, base::ScopedFD>;

PathAndFd FindDrmDriverOrDie(std::string name);

}  // namespace ui::test

#endif  // UI_OZONE_PLATFORM_DRM_TEST_INTEGRATION_TEST_HELPERS_H_
