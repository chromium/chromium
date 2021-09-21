// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_LAYOUT_STORE_H_
#define UI_DISPLAY_MANAGER_DISPLAY_LAYOUT_STORE_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
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

  // If no layout is registered, it creatas new layout using
  // |default_display_layout_|.
  const DisplayLayout& GetRegisteredDisplayLayout(const DisplayIdList& list);

  // Update the default unified desktop mode in the display layout for
  // |display_list|.  This creates new display layout if no layout is
  // registered for |display_list|.
  void UpdateDefaultUnified(const DisplayIdList& display_list,
                            bool default_unified);

 private:
  // Creates new layout for display list from |default_display_layout_|.
  DisplayLayout* CreateDefaultDisplayLayout(const DisplayIdList& display_list);

  // The default display placement.
  DisplayPlacement default_display_placement_;

  bool forced_mirror_mode_for_tablet_ = false;

  // Display layout per list of devices.
  std::map<DisplayIdList, std::unique_ptr<DisplayLayout>> layouts_;
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_LAYOUT_STORE_H_
