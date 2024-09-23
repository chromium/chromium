// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/modifier_split_dogfood_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ui {

ModifierSplitDogfoodController::ModifierSplitDogfoodController() {
  // Dogfood flag should be ignored and not considered if the secret key
  // matches.
  modifier_split_enabled_ = ash::features::IsModifierSplitEnabled() &&
                            ash::switches::IsModifierSplitSecretKeyMatched();

  if (user_manager::UserManager::IsInitialized() &&
      ash::features::IsModifierSplitEnabled()) {
    user_manager::UserManager::Get()->AddObserver(this);
  }
}

ModifierSplitDogfoodController::~ModifierSplitDogfoodController() {
  if (user_manager::UserManager::IsInitialized() &&
      ash::features::IsModifierSplitEnabled()) {
    user_manager::UserManager::Get()->RemoveObserver(this);
  }
}

void ModifierSplitDogfoodController::ForceEnableFeature() {
  modifier_split_enabled_ = true;
}

void ModifierSplitDogfoodController::OnUserLoggedIn(
    const user_manager::User& user) {
  if (modifier_split_enabled_) {
    return;
  }

  if (!::ash::features::IsModifierSplitDogfoodEnabled()) {
    return;
  }

  const auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user) {
    return;
  }

  modifier_split_enabled_ = gaia::IsGoogleInternalAccountEmail(
      primary_user->GetAccountId().GetUserEmail());
}

}  // namespace ui
