// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_LAYOUT_STORE_H_
#define UI_DISPLAY_MANAGER_DISPLAY_LAYOUT_STORE_H_

#include <stdint.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_export.h"

namespace display {

class DISPLAY_MANAGER_EXPORT DisplayLayoutStore {
 public:
  DisplayLayoutStore();

  DisplayLayoutStore(const DisplayLayoutStore&) = delete;
  DisplayLayoutStore& operator=(const DisplayLayoutStore&) = delete;

  ~DisplayLayoutStore();

  // Set true to force mirror mode. This should only be used when tablet mode is
  // turned on/off.
  void set_forced_mirror_mode_for_tablet(bool forced) {
    forced_mirror_mode_for_tablet_ = forced;
  }

  bool forced_mirror_mode_for_tablet() const {
    return forced_mirror_mode_for_tablet_;
  }

  void SetDefaultDisplayPlacement(const DisplayPlacement& placement);

  // Registers the display layout info for the specified display(s).
  void RegisterLayoutForDisplayIdList(const DisplayIdList& list,
                                      std::unique_ptr<DisplayLayout> layout);

  // Returns a layout for given `display_id_list` or fails with DCHECK if not
  // exist.
  const DisplayLayout& GetRegisteredDisplayLayout(const DisplayIdList& list);

  // Update the default unified desktop mode in the display layout for
  // |display_list|.
  void UpdateDefaultUnified(const DisplayIdList& display_list,
                            bool default_unified);

 private:
  friend void DisplayManager::UpdateDisplaysWith(
      const std::vector<ManagedDisplayInfo>& updated_display_info_list);

  // Returns a layout for the given `display_id_list` or create one if no layout
  // has been registered. This should be created at the designated point only
  // (currently in `DisplayManager::UpdateDisplayWith()` to ensure we do not
  // generate a new layout in random places. This is private so that only
  // `DisplayManager::UpdateDisplaysWith()` can call this method.
  const DisplayLayout& GetOrCreateRegisteredDisplayLayout(
      const DisplayIdList& display_id_list);

  // Creates new layout for display list from |default_display_layout_|.
  DisplayLayout* CreateDefaultDisplayLayout(const DisplayIdList& display_list);

  const DisplayLayout& GetOrCreateRegisteredDisplayLayoutInternal(
      const DisplayIdList& list,
      bool create_if_not_exist);

  // The default display placement.
  DisplayPlacement default_display_placement_;

  bool forced_mirror_mode_for_tablet_ = false;

  // Display layout per list of devices.
  base::flat_map<DisplayIdList, std::unique_ptr<DisplayLayout>> layouts_;
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_LAYOUT_STORE_H_
