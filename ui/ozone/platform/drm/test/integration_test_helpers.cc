// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/test/integration_test_helpers.h"

#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/libdrm/src/xf86drm.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"

namespace ui::test {

PathAndFd FindDrmDriverOrDie(std::string name) {
  constexpr char kDefaultGraphicsCardPattern[] = "/dev/dri/card%d";

  std::vector<std::string> seen_drivers;

  // Loop over all cards until we find a VKMS driver, or die when we run out of
  // things to look at. Drivers may be in any order.
  for (int i = 0;; i++) {
    std::string card_path = base::StringPrintf(kDefaultGraphicsCardPattern, i);

    if (access(card_path.c_str(), F_OK) != 0)
      LOG(FATAL) << "Unable to find a suitable " << name
                 << " driver. Saw: " << base::JoinString(seen_drivers, ", ");

    base::ScopedFD fd(open(card_path.c_str(), O_RDWR | O_CLOEXEC));
    if (!fd.is_valid())
      continue;

    ui::ScopedDrmVersionPtr version(drmGetVersion(fd.get()));
    auto driver_name = std::string(version->name, version->name_len);
    if (driver_name != name) {
      seen_drivers.push_back(driver_name);
      continue;
    }

    return {base::FilePath(card_path), std::move(fd)};
  }
}

}  // namespace ui::test
