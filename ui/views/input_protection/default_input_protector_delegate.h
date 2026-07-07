// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_DEFAULT_INPUT_PROTECTOR_DELEGATE_H_
#define UI_VIEWS_INPUT_PROTECTION_DEFAULT_INPUT_PROTECTOR_DELEGATE_H_

#include "ui/views/input_protection/input_protector_delegate.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// Default implementation of `InputProtectorDelegate` that implements the
// standard timing-based protection (cooldown and click-rate limits). This
// delegate is used by default when no custom delegate is passed to the
// `InputEventActivationProtector` constructor.
class VIEWS_EXPORT DefaultInputProtectorDelegate
    : public InputProtectorDelegate {
 public:
  DefaultInputProtectorDelegate() = default;
  ~DefaultInputProtectorDelegate() override = default;

  // InputProtectorDelegate:
  bool IsPossiblyUnintendedInteraction(
      const ui::Event& event,
      const View* target_view,
      InputEventActivationProtector* protector) override;
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_DEFAULT_INPUT_PROTECTOR_DELEGATE_H_
