// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"

struct wl_output;

namespace ui {

class WaylandConnection;
class WaylandOutput;

class WaylandOutputManager : public WaylandOutput::Delegate {
 public:
  explicit WaylandOutputManager(WaylandConnection* connection);

  WaylandOutputManager(const WaylandOutputManager&) = delete;
  WaylandOutputManager& operator=(const WaylandOutputManager&) = delete;

  ~WaylandOutputManager() override;

  // Says if at least one output has already been announced by a Wayland
  // compositor.
  bool IsOutputReady() const;

  void AddWaylandOutput(const uint32_t output_id, wl_output* output);
  void RemoveWaylandOutput(const uint32_t output_id);

  void InitializeAllXdgOutputs();
  void InitializeAllZAuraOutputs();
  void InitializeAllColorManagementOutputs();

  // Creates a platform screen.
  std::unique_ptr<WaylandScreen> CreateWaylandScreen();

  // Feeds a new platform screen with existing outputs.
  void InitWaylandScreen(WaylandScreen* screen);

  WaylandOutput* GetOutput(uint32_t id) const;
  WaylandOutput* GetPrimaryOutput() const;

  WaylandScreen* wayland_screen() const { return wayland_screen_.get(); }

 private:
  // WaylandOutput::Delegate:
  void OnOutputHandleMetrics(uint32_t output_id,
                             const gfx::Point& origin,
                             const gfx::Size& logical_size,
                             const gfx::Size& physical_size,
                             const gfx::Insets& insets,
                             float scale_factor,
                             int32_t panel_transform,
                             int32_t logical_transform,
                             const std::string& label) override;

  using OutputList = base::flat_map<uint32_t, std::unique_ptr<WaylandOutput>>;

  OutputList output_list_;

  const raw_ptr<WaylandConnection> connection_;

  // Non-owned wayland screen instance.
  base::WeakPtr<WaylandScreen> wayland_screen_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_MANAGER_H_
