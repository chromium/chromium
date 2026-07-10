// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_OCCLUSION_AWARE_INPUT_PROTECTION_DELEGATE_H_
#define UI_VIEWS_INPUT_PROTECTION_OCCLUSION_AWARE_INPUT_PROTECTION_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/input_protection/input_protector_delegate.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

class InputEventActivationProtector;
class Widget;
class View;

// An implementation of `InputProtectorDelegate` that protects widgets against
// unintended interaction caused by occlusion from always-on-top windows
// (current or recent).
class VIEWS_EXPORT OcclusionAwareInputProtectionDelegate final
    : public InputProtectorDelegate,
      public views::WidgetObserver {
 public:
  explicit OcclusionAwareInputProtectionDelegate(Widget* widget);
  ~OcclusionAwareInputProtectionDelegate() override;

  OcclusionAwareInputProtectionDelegate(
      const OcclusionAwareInputProtectionDelegate&) = delete;
  OcclusionAwareInputProtectionDelegate& operator=(
      const OcclusionAwareInputProtectionDelegate&) = delete;

  // InputProtectorDelegate:
  bool IsPossiblyUnintendedInteraction(
      const ui::Event& event,
      const View* target_view,
      InputEventActivationProtector* protector) override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(Widget* widget, bool active) override;
  void OnWidgetDestroying(Widget* widget) override;

 private:
  raw_ptr<Widget> widget_;
  base::ScopedObservation<Widget, WidgetObserver> observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_OCCLUSION_AWARE_INPUT_PROTECTION_DELEGATE_H_
