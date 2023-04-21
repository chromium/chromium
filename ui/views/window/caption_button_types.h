// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_CAPTION_BUTTON_TYPES_H_
#define UI_VIEWS_WINDOW_CAPTION_BUTTON_TYPES_H_

namespace views {

// These are the icon types that a caption button can have. The size button's
// action (SnapType) can be different from its icon.
enum CaptionButtonIcon {
  CAPTION_BUTTON_ICON_MINIMIZE,
  CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
  CAPTION_BUTTON_ICON_CLOSE,
  CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
  CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
  CAPTION_BUTTON_ICON_BACK,
  CAPTION_BUTTON_ICON_LOCATION,
  CAPTION_BUTTON_ICON_MENU,
  CAPTION_BUTTON_ICON_ZOOM,
  CAPTION_BUTTON_ICON_CENTER,
  CAPTION_BUTTON_ICON_FLOAT,
  // The custom icon type allows clients to instantiate a caption button that is
  // specific to their use case (e.g. tab search caption button in the browser
  // window frame).
  CAPTION_BUTTON_ICON_CUSTOM,
  CAPTION_BUTTON_ICON_COUNT,
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_CAPTION_BUTTON_TYPES_H_
