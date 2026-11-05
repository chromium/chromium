// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/anchored_dialog_registry.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

namespace ui {

AnchoredDialogRegistry::AnchoredDialogRegistry() = default;
AnchoredDialogRegistry::~AnchoredDialogRegistry() = default;

// static
AnchoredDialogRegistry* AnchoredDialogRegistry::GetInstance() {
  static base::NoDestructor<AnchoredDialogRegistry> instance;
  return instance.get();
}

// static
std::unique_ptr<AnchoredDialogRegistry>
AnchoredDialogRegistry::MakeInstanceForTesting() {
  return base::WrapUnique(new AnchoredDialogRegistry());
}

std::unique_ptr<AnchoredDialogRegistry::Registration>
AnchoredDialogRegistry::Register(TrackedElement* anchor, FocusTarget& target) {
  if (!anchor) {
    return nullptr;
  }

  for (auto& backend : backends_) {
    if (std::unique_ptr<Registration> registration =
            backend.Register(*anchor, target)) {
      return registration;
    }
  }
  return nullptr;
}

}  // namespace ui
