// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_MANAGER_H_

#include <memory>
#include <ostream>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"

struct wl_output;

namespace ui {

class WaylandConnection;
class WaylandOutput;

class WaylandOutputManager : public WaylandOutput::Delegate {
 public:
  using OutputList =
      base::flat_map<WaylandOutput::Id, std::unique_ptr<WaylandOutput>>;

  explicit WaylandOutputManager(WaylandConnection* connection);

  WaylandOutputManager(const WaylandOutputManager&) = delete;
  WaylandOutputManager& operator=(const WaylandOutputManager&) = delete;

  ~WaylandOutputManager() override;

  // Says if at least one output has already been announced by a Wayland
  // compositor.
  bool IsOutputReady() const;

  void AddWaylandOutput(WaylandOutput::Id output_id, wl_output* output);
  void RemoveWaylandOutput(WaylandOutput::Id output_id);

  void InitializeAllXdgOutputs();
  void InitializeAllColorManagementOutputs();

  // Creates a platform screen.
  std::unique_ptr<WaylandScreen> CreateWaylandScreen();

  // Feeds a new platform screen with existing outputs.
  void InitWaylandScreen(WaylandScreen* screen);

  // Returns the output_id (i.e. the output's assigned global name).
  WaylandOutput::Id GetOutputId(wl_output* output_resource) const;
  WaylandOutput* GetOutput(WaylandOutput::Id id) const;
  WaylandOutput* GetPrimaryOutput() const;
  const OutputList& GetAllOutputs() const;

  WaylandScreen* wayland_screen() const { return wayland_screen_.get(); }

  void DumpState(std::ostream& out) const;

 private:
  // WaylandOutput::Delegate:
  void OnOutputHandleMetrics(const WaylandOutput::Metrics& metrics) override;

  OutputList output_list_;

  const raw_ptr<WaylandConnection> connection_;

  // Non-owned wayland screen instance.
  base::WeakPtr<WaylandScreen> wayland_screen_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_MANAGER_H_
