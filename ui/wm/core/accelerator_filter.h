// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_ACCELERATOR_FILTER_H_
#define UI_WM_CORE_ACCELERATOR_FILTER_H_

#include <memory>

#include "base/component_export.h"
#include "ui/events/event_handler.h"

namespace wm {
class AcceleratorDelegate;

// AcceleratorFilter filters key events for AcceleratorControler handling global
// keyboard accelerators.
class COMPONENT_EXPORT(UI_WM) AcceleratorFilter : public ui::EventHandler {
 public:
  // AcceleratorFilter doesn't own |accelerator_history|, it's owned by
  // AcceleratorController.
  explicit AcceleratorFilter(std::unique_ptr<AcceleratorDelegate> delegate);

  AcceleratorFilter(const AcceleratorFilter&) = delete;
  AcceleratorFilter& operator=(const AcceleratorFilter&) = delete;

  ~AcceleratorFilter() override;

  // If the return value is true, |event| should be filtered out.
  static bool ShouldFilter(ui::KeyEvent* event);

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  std::unique_ptr<AcceleratorDelegate> delegate_;
};

}  // namespace wm

#endif  // UI_WM_CORE_ACCELERATOR_FILTER_H_
