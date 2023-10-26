// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_TOOLTIP_MANAGER_H_
#define UI_VIEWS_WIDGET_TOOLTIP_MANAGER_H_


#include "ui/views/views_export.h"

namespace gfx {
class FontList;
class Point;
}  // namespace gfx

namespace views {

class View;

// TooltipManager takes care of the wiring to support tooltips for Views. You
// almost never need to interact directly with TooltipManager, rather look to
// the various tooltip methods on View.
class VIEWS_EXPORT TooltipManager {
 public:
  // When a NativeView has capture all events are delivered to it. In some
  // situations, such as menus, we want the tooltip to be shown for the
  // NativeView the mouse is over, even if it differs from the NativeView that
  // has capture (with menus the first menu shown has capture). To enable this
  // if the NativeView that has capture has the same value for the property
  // |kGroupingPropertyKey| as the NativeView the mouse is over the tooltip is
  // shown.
  static const char kGroupingPropertyKey[];

  TooltipManager() = default;
  virtual ~TooltipManager() = default;

  // Returns the maximum width of the tooltip. |point| gives the location
  // the tooltip is to be displayed on in screen coordinates.
  virtual int GetMaxWidth(const gfx::Point& location) const = 0;

  // Returns the font list used for tooltips.
  virtual const gfx::FontList& GetFontList() const = 0;

  // Notification that the view hierarchy has changed in some way.
  virtual void UpdateTooltip() = 0;
  virtual void UpdateTooltipForFocus(View* view) = 0;

  // Invoked when the tooltip text changes for the specified views.
  virtual void TooltipTextChanged(View* view) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_TOOLTIP_MANAGER_H_
