// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_MODIFIER_SPLIT_DOGFOOD_CONTROLLER_H_
#define UI_EVENTS_ASH_MODIFIER_SPLIT_DOGFOOD_CONTROLLER_H_

#include "components/user_manager/user_manager.h"

namespace ui {

// TODO(b/339873990): Clean this up once key is no longer guarding the feature.
// Tracks whether the modifier split feature should be enabled in all UI visible
// ways. It can either be enabled directly via flag + key or via dogfood flag +
// internal google account.
class ModifierSplitDogfoodController
    : public user_manager::UserManager::Observer {
 public:
  ModifierSplitDogfoodController();
  ModifierSplitDogfoodController(const ModifierSplitDogfoodController&) =
      delete;
  ModifierSplitDogfoodController& operator=(
      const ModifierSplitDogfoodController&) = delete;
  ~ModifierSplitDogfoodController() override;

  bool IsEnabled() const { return modifier_split_enabled_; }

  void ForceEnableFeature();

  // user_manager::UserManager::Observer:
  void OnUserLoggedIn(const user_manager::User& user) override;

 private:
  bool modifier_split_enabled_ = false;
};

}  // namespace ui

#endif  // UI_EVENTS_ASH_MODIFIER_SPLIT_DOGFOOD_CONTROLLER_H_
