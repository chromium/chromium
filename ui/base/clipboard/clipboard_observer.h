// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_OBSERVER_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_OBSERVER_H_

#include "base/component_export.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace ui {

// Observer that receives the notifications of clipboard events.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ClipboardObserver {
 public:
  // Override notified when clipboard data is changed.
  virtual void OnClipboardDataChanged();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Override notified when clipboard data is read.
  virtual void OnClipboardDataRead();
#endif

 protected:
  virtual ~ClipboardObserver();
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_OBSERVER_H_
