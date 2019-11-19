// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_OBSERVER_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_OBSERVER_H_

#include "base/component_export.h"
#include "base/macros.h"

namespace ui {

// Observer that receives the notifications of clipboard change events.
class COMPONENT_EXPORT(BASE_CLIPBOARD) ClipboardObserver {
 public:
  // Called when clipboard data is changed.
  virtual void OnClipboardDataChanged() = 0;

 protected:
  virtual ~ClipboardObserver() = default;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_OBSERVER_H_
