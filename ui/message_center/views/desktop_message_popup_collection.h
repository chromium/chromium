// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_DESKTOP_MESSAGE_POPUP_COLLECTION_H_
#define UI_MESSAGE_CENTER_VIEWS_DESKTOP_MESSAGE_POPUP_COLLECTION_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/views/message_popup_collection.h"

namespace display {
class Screen;
}

namespace message_center {
namespace test {
class MessagePopupCollectionTest;
}

// The MessagePopupCollection for non-ash Windows/Linux desktop.
class MESSAGE_CENTER_EXPORT DesktopMessagePopupCollection
    : public MessagePopupCollection,
      public display::DisplayObserver {
 public:
  DesktopMessagePopupCollection();

  DesktopMessagePopupCollection(const DesktopMessagePopupCollection&) = delete;
  DesktopMessagePopupCollection& operator=(
      const DesktopMessagePopupCollection&) = delete;

  ~DesktopMessagePopupCollection() override;

  void StartObserving();

  // Overridden from MessagePopupCollection:
  bool RecomputeAlignment(const display::Display& display) override;
  void ConfigureWidgetInitParamsForContainer(
      views::Widget* widget,
      views::Widget::InitParams* init_params) override;

 protected:
  // Overridden from MessagePopupCollection:
  int GetPopupOriginX(const gfx::Rect& popup_bounds) const override;
  int GetBaseline() const override;
  gfx::Rect GetWorkArea() const override;
  bool IsTopDown() const override;
  bool IsFromLeft() const override;
  bool IsPrimaryDisplayForNotification() const override;
  bool BlockForMixedFullscreen(const Notification& notification) const override;

 private:
  friend class test::MessagePopupCollectionTest;

  enum PopupAlignment {
    POPUP_ALIGNMENT_TOP = 1 << 0,
    POPUP_ALIGNMENT_LEFT = 1 << 1,
    POPUP_ALIGNMENT_BOTTOM = 1 << 2,
    POPUP_ALIGNMENT_RIGHT = 1 << 3,
  };

  void UpdatePrimaryDisplay();

  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  int32_t alignment_ = POPUP_ALIGNMENT_BOTTOM | POPUP_ALIGNMENT_RIGHT;
  int64_t primary_display_id_ = display::kInvalidDisplayId;
  raw_ptr<display::Screen> screen_ = nullptr;
  std::optional<display::ScopedDisplayObserver> display_observer_;
  gfx::Rect work_area_;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_DESKTOP_MESSAGE_POPUP_COLLECTION_H_
