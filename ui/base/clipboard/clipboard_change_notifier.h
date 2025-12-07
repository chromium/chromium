// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_CHANGE_NOTIFIER_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_CHANGE_NOTIFIER_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace ui {

// Implemented by each platform specific clipboard implementation which extends
// ui::Clipboard.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ClipboardChangeNotifier {
 public:
  virtual void StartNotifying();
  virtual void StopNotifying();
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_CHANGE_NOTIFIER_H_
