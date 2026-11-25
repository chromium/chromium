// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/tracked_element/anchored_dialog_registry_webui.h"

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace ui {

class AnchoredDialogRegistryWebUI::RegistrationImpl
    : public AnchoredDialogRegistry::Registration {
 public:
  RegistrationImpl(TrackedElementWebUI& element,
                   AnchoredDialogRegistry::FocusTarget& target)
      : element_(element.GetWeakPtr()), target_(target) {
    element.AddAnchoredDialog(target);
  }

  ~RegistrationImpl() override {
    if (element_) {
      element_->RemoveAnchoredDialog(*target_);
    }
  }

 private:
  // The element goes away when it is no longer visible in the WebUI, which
  // also takes the dialog out of the focus order.
  base::WeakPtr<TrackedElementWebUI> element_;
  const raw_ref<AnchoredDialogRegistry::FocusTarget> target_;
};

AnchoredDialogRegistryWebUI::AnchoredDialogRegistryWebUI() = default;
AnchoredDialogRegistryWebUI::~AnchoredDialogRegistryWebUI() = default;

std::unique_ptr<AnchoredDialogRegistry::Registration>
AnchoredDialogRegistryWebUI::Register(
    TrackedElement& anchor,
    AnchoredDialogRegistry::FocusTarget& target) {
  auto* const element = anchor.AsA<TrackedElementWebUI>();
  if (!element) {
    return nullptr;
  }
  return std::make_unique<RegistrationImpl>(*element, target);
}

DEFINE_SAFE_CAST_TARGET(AnchoredDialogRegistryWebUI)

}  // namespace ui
