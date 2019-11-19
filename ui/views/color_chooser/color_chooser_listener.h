// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COLOR_CHOOSER_COLOR_CHOOSER_LISTENER_H_
#define UI_VIEWS_COLOR_CHOOSER_COLOR_CHOOSER_LISTENER_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/views_export.h"

namespace views {

// An interface implemented by a Listener object wishing to know about the
// the results from the color chooser dialog.
class VIEWS_EXPORT ColorChooserListener {
 public:
  virtual void OnColorChosen(SkColor color) = 0;
  virtual void OnColorChooserDialogClosed() = 0;

 protected:
  virtual ~ColorChooserListener() = default;
};

}  // namespace views

#endif  // UI_VIEWS_COLOR_CHOOSER_COLOR_CHOOSER_LISTENER_H_
