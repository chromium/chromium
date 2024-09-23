// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_PORT_OBSERVER_H_
#define UI_DISPLAY_MANAGER_DISPLAY_PORT_OBSERVER_H_

#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/manager/display_configurator.h"

namespace display {
// DisplayPortObserver keeps track of which USB-C ports are used for displays
// and notifies Type C Daemon in ChromiumOS.
class DISPLAY_MANAGER_EXPORT DisplayPortObserver
    : public DisplayConfigurator::Observer {
 public:
  explicit DisplayPortObserver(
      DisplayConfigurator* configurator,
      base::RepeatingCallback<void(const std::vector<uint32_t>&)>
          on_port_change_callback);
  DisplayPortObserver(const DisplayPortObserver&) = delete;
  DisplayPortObserver& operator=(const DisplayPortObserver&) = delete;
  ~DisplayPortObserver() override;

  // Overridden from DisplayConfigurator::Observer:
  void OnDisplayConfigurationChanged(
      const DisplayConfigurator::DisplayStateList& outputs) override;
  void OnDisplayConfigurationChangeFailed(
      const DisplayConfigurator::DisplayStateList& displays,
      MultipleDisplayState failed_new_state) override;

 private:
  friend class DisplayChangeObserverTest;

  void SetTypeCPortsUsingDisplays(std::vector<uint32_t> port_nums);

  const raw_ptr<DisplayConfigurator> configurator_;

  // Used to determine if there is a change in ports.
  std::set<uint64_t> prev_base_connector_ids_;

  // Callback function to be called by OnDisplayConfigurationChanged. The
  // parameter is a list of port numbers that has displays connected. This shall
  // be initialized and set by ash to call a D-bus method that notifies ChromeOS
  // Type C Daemon on which ports are driving displays.
  const base::RepeatingCallback<void(const std::vector<uint32_t>&)>
      on_port_change_callback_;

  base::WeakPtrFactory<DisplayPortObserver> weak_ptr_factory_{this};
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_PORT_OBSERVER_H_
