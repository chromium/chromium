// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/common/drm_render_node_path_finder.h"

#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/linux/scoped_gbm_device.h"  // nogncheck
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

DrmRenderNodePathFinder::DrmRenderNodePathFinder() {
  FindDrmRenderNodePath();
}

DrmRenderNodePathFinder::~DrmRenderNodePathFinder() = default;

base::FilePath DrmRenderNodePathFinder::GetDrmRenderNodePath() const {
  return drm_render_node_path_;
}

void DrmRenderNodePathFinder::FindDrmRenderNodePath() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRenderNodeOverride)) {
    drm_render_node_path_ =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kRenderNodeOverride);
    return;
  }

  int max_devices = drmGetDevices2(0, nullptr, 0);
  if (max_devices <= 0) {
    PLOG(ERROR) << "drmGetDevices2() has not found any devices";
    return;
  }

  std::vector<drmDevicePtr> devices{static_cast<size_t>(max_devices), nullptr};
  int ret = drmGetDevices2(0, devices.data(), max_devices);
  if (ret < 0) {
    PLOG(ERROR) << "drmGetDevices2() returned an error";
    return;
  }

  std::vector<std::pair<std::string, std::string>> driver_to_nodes;
  for (const auto& device : devices) {
    if (!device || !(device->available_nodes & 1 << DRM_NODE_RENDER)) {
      continue;
    }

    CHECK(device->nodes);
    const std::string dri_render_node(
        UNSAFE_TODO(device->nodes[DRM_NODE_RENDER]));
    base::ScopedFD drm_fd(open(dri_render_node.c_str(), O_RDWR));
    if (drm_fd.get() < 0) {
      continue;
    }

    drmVersionPtr version = drmGetVersion(drm_fd.get());
    if (!version) {
      continue;
    }
    const std::string driver_name(version->name, version->name_len);
    drmFreeVersion(version);
    // Skip if this is the vgem render node.
    if (driver_name == "vgem") {
      continue;
    }

    // In case the first node /dev/dri/renderD128 can be opened but fails to
    // create gbm device on certain driver (E.g. PowerVR). Skip such paths.
    {
      TRACE_EVENT("gpu,startup", "scoped attempt of gbm_create_device");
      ScopedGbmDevice gbm_device(gbm_create_device(drm_fd.get()));
      if (!gbm_device) {
        continue;
      }
    }

    driver_to_nodes.emplace_back(driver_name, dri_render_node);
  }
  drmFreeDevices(devices.data(), max_devices);
  devices.clear();

  if (driver_to_nodes.empty()) {
    return;
  }

  static constexpr const char* preferred_drivers[3] = {"i915", "amdgpu",
                                                       "virtio_gpu"};
  for (const char* preferred_driver : preferred_drivers) {
    for (const auto& [driver, node] : driver_to_nodes) {
      if (driver == preferred_driver) {
        drm_render_node_path_ = base::FilePath(node);
        return;
      }
    }
  }

  LOG(WARNING) << "Preferred drm_render_node not found, picking "
               << driver_to_nodes[0].first;
  drm_render_node_path_ = base::FilePath(driver_to_nodes[0].second);
}

}  // namespace ui
