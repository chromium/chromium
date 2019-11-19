// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_WIN_H_
#define UI_VIEWS_COREWM_TOOLTIP_WIN_H_

#include <windows.h>
#include <commctrl.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/win/scoped_gdi_object.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/corewm/tooltip.h"

namespace views {
namespace corewm {

// Implementation of Tooltip that uses the native win32 control for showing the
// tooltip.
class VIEWS_EXPORT TooltipWin : public Tooltip {
 public:
  explicit TooltipWin(HWND parent);
  ~TooltipWin() override;

  // HandleNotify() is forwarded from DesktopWindowTreeHostWin to keep the
  // native tooltip in sync.
  bool HandleNotify(int w_param, NMHDR* l_param, LRESULT* l_result);

 private:
  // Ensures |tooltip_hwnd_| is valid. Returns true if valid, false if there
  // a problem creating |tooltip_hwnd_|.
  bool EnsureTooltipWindow();

  // Sets the position of the tooltip.
  void PositionTooltip();

  // Might override the font size for localization (e.g. Hindi).
  void MaybeOverrideFont();

  // Tooltip:
  int GetMaxWidth(const gfx::Point& location) const override;
  void SetText(aura::Window* window,
               const base::string16& tooltip_text,
               const gfx::Point& location) override;
  void Show() override;
  void Hide() override;
  bool IsVisible() override;

  // Font we're currently overriding our UI font with.
  // Should outlast |tooltip_hwnd_|.
  base::win::ScopedHFONT override_font_;

  // The window |tooltip_hwnd_| is parented to.
  HWND parent_hwnd_;

  // Shows the tooltip.
  HWND tooltip_hwnd_;

  // Used to modify the tooltip.
  TOOLINFO toolinfo_;

  // Is the tooltip showing?
  bool showing_;

  // Location to show the tooltip at. In order to position the tooltip we need
  // to know the size. The size is only available from TTN_SHOW, so we have to
  // cache it.
  gfx::Point location_;

  // What the scale was the last time we overrode the font, to see if we can
  // re-use our previous override.
  float override_scale_ = 0.0f;

  DISALLOW_COPY_AND_ASSIGN(TooltipWin);
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TOOLTIP_WIN_H_
