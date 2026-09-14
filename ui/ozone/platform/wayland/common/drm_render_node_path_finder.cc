// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/common/drm_render_node_path_finder.h"

#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/linux/scoped_gbm_device.h"  // nogncheck
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

// Scoped RAII wrapper around drmGetDevices2() and drmFreeDevices().
class ScopedDrmDeviceList {
 public:
  ScopedDrmDeviceList() {
    int max_devices = drmGetDevices2(0, nullptr, 0);
    if (max_devices <= 0) {
      PLOG(ERROR) << "drmGetDevices2() has not found any devices";
      return;
    }

    devices_.resize(static_cast<size_t>(max_devices), nullptr);
    int ret = drmGetDevices2(0, devices_.data(), max_devices);
    if (ret < 0) {
      PLOG(ERROR) << "drmGetDevices2() returned an error";
      devices_.clear();
      return;
    }

    count_ = static_cast<size_t>(ret);
  }

  ~ScopedDrmDeviceList() {
    if (count_ > 0) {
      drmFreeDevices(devices_.data(), static_cast<int>(count_));
    }
  }

  ScopedDrmDeviceList(const ScopedDrmDeviceList&) = delete;
  ScopedDrmDeviceList& operator=(const ScopedDrmDeviceList&) = delete;

  // Returns a span containing only the populated devices.
  base::span<const drmDevice* const> devices() const {
    return base::span(devices_).first(count_);
  }

 private:
  std::vector<drmDevicePtr> devices_;
  size_t count_ = 0;
};

// In libdrm xf86drm.h, DRM_NODE_MAX and DRM_NODE_RENDER are legacy macro
// constants. Provide typed compile-time constants for span bounds checking.
constexpr size_t kDrmNodeMax = base::checked_cast<size_t>(DRM_NODE_MAX);
constexpr size_t kDrmNodeRender = base::checked_cast<size_t>(DRM_NODE_RENDER);
static_assert(kDrmNodeRender < kDrmNodeMax);

// Safely extracts the render node path from a drmDevice as a base::FilePath.
std::optional<base::FilePath> GetRenderNodePath(const drmDevice& device) {
  if (!(device.available_nodes & (1 << kDrmNodeRender))) {
    return std::nullopt;
  }
  CHECK(device.nodes);
  // SAFETY: libdrm defines `nodes` as an array of `DRM_NODE_MAX` elements.
  // `kDrmNodeRender` is statically asserted to be in-bounds (< kDrmNodeMax).
  auto nodes = UNSAFE_BUFFERS(base::span(device.nodes, kDrmNodeMax));
  const char* path_str = nodes[kDrmNodeRender];
  if (!path_str) {
    return std::nullopt;
  }
  return base::FilePath(path_str);
}

using ScopedDrmVersion = std::unique_ptr<drmVersion, decltype(&drmFreeVersion)>;

// Probes available DRM devices, validates they can be opened and initialized
// with GBM, and returns all viable candidates.
std::vector<DrmRenderNodeCandidate> ProbeRenderNodes() {
  ScopedDrmDeviceList device_list;
  if (device_list.devices().empty()) {
    return {};
  }

  std::vector<DrmRenderNodeCandidate> candidates;
  for (const drmDevice* device : device_list.devices()) {
    if (!device) {
      continue;
    }

    std::optional<base::FilePath> path = GetRenderNodePath(*device);
    if (!path) {
      continue;
    }

    base::ScopedFD drm_fd(open(path->value().c_str(), O_RDWR));
    if (!drm_fd.is_valid()) {
      continue;
    }

    ScopedDrmVersion version(drmGetVersion(drm_fd.get()), &drmFreeVersion);
    if (!version) {
      continue;
    }

    std::string_view driver_name(version->name, version->name_len);
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

    candidates.push_back({std::string(driver_name), *std::move(path)});
  }

  return candidates;
}

}  // namespace

DrmRenderNodePathFinder::DrmRenderNodePathFinder() {
  FindDrmRenderNodePath();
}

DrmRenderNodePathFinder::~DrmRenderNodePathFinder() = default;

base::FilePath DrmRenderNodePathFinder::GetDrmRenderNodePath() const {
  return drm_render_node_path_;
}

// static
base::FilePath DrmRenderNodePathFinder::SelectRenderNodeFromCandidates(
    base::span<const DrmRenderNodeCandidate> candidates) {
  if (candidates.empty()) {
    return base::FilePath();
  }

  static constexpr std::string_view kPreferredDrivers[] = {"i915", "amdgpu",
                                                           "virtio_gpu"};
  for (std::string_view preferred_driver : kPreferredDrivers) {
    for (const auto& candidate : candidates) {
      if (candidate.driver_name == preferred_driver) {
        return candidate.path;
      }
    }
  }

  LOG(WARNING) << "Preferred drm_render_node not found, picking "
               << candidates.front().driver_name;
  return candidates.front().path;
}

void DrmRenderNodePathFinder::FindDrmRenderNodePath() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRenderNodeOverride)) {
    drm_render_node_path_ =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kRenderNodeOverride);
    return;
  }

  std::vector<DrmRenderNodeCandidate> candidates = ProbeRenderNodes();
  drm_render_node_path_ = SelectRenderNodeFromCandidates(candidates);
}

}  // namespace ui
