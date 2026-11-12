// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_TRACKED_ELEMENT_ANCHORED_DIALOG_REGISTRY_WEBUI_H_
#define UI_WEBUI_TRACKED_ELEMENT_ANCHORED_DIALOG_REGISTRY_WEBUI_H_

#include <memory>

#include "ui/base/interaction/anchored_dialog_registry.h"

namespace ui {

// Inserts dialogs anchored to WebUI elements into the WebUI's sequential focus
// order, through the mojo connection managed by TrackedElementHandler (see
// TrackedElementManager.OnElementAnchoredDialogChanged()).
// TrackedElementHandler installs this.
class AnchoredDialogRegistryWebUI : public AnchoredDialogRegistry::Backend {
 public:
  AnchoredDialogRegistryWebUI();
  ~AnchoredDialogRegistryWebUI() override;

  // AnchoredDialogRegistry::Backend:
  std::unique_ptr<AnchoredDialogRegistry::Registration> Register(
      TrackedElement& anchor,
      AnchoredDialogRegistry::FocusTarget& target) override;

  DECLARE_SAFE_CAST_TARGET()

 private:
  class RegistrationImpl;
};

}  // namespace ui

#endif  // UI_WEBUI_TRACKED_ELEMENT_ANCHORED_DIALOG_REGISTRY_WEBUI_H_
