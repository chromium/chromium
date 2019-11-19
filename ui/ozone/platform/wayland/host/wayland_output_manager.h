// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_MANAGER_H_

#include "ui/ozone/platform/wayland/common/wayland_object.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"

struct wl_output;

namespace ui {

class WaylandConnection;
class WaylandOutput;

class WaylandOutputManager : public WaylandOutput::Delegate {
 public:
  WaylandOutputManager();
  ~WaylandOutputManager() override;

  // Says if at least one output has already been announced by a Wayland
  // compositor.
  bool IsOutputReady() const;

  void AddWaylandOutput(const uint32_t output_id, wl_output* output);
  void RemoveWaylandOutput(const uint32_t output_id);

  // Creates a platform screen and feeds it with existing outputs.
  std::unique_ptr<WaylandScreen> CreateWaylandScreen(
      WaylandConnection* connection);

  uint32_t GetIdForOutput(wl_output* output) const;
  WaylandOutput* GetOutput(uint32_t id) const;

  WaylandScreen* wayland_screen() const { return wayland_screen_.get(); }

 private:
  void OnWaylandOutputAdded(uint32_t output_id);
  void OnWaylandOutputRemoved(uint32_t output_id);

  // WaylandOutput::Delegate:
  void OnOutputHandleMetrics(uint32_t output_id,
                             const gfx::Rect& new_bounds,
                             int32_t scale_factor) override;

  using OutputList = std::vector<std::unique_ptr<WaylandOutput>>;

  OutputList::const_iterator GetOutputItById(uint32_t id) const;

  OutputList output_list_;

  // Non-owned wayland screen instance.
  base::WeakPtr<WaylandScreen> wayland_screen_;

  DISALLOW_COPY_AND_ASSIGN(WaylandOutputManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_MANAGER_H_
