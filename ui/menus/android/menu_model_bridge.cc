// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/menus/android/menu_model_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/menu_model_bridge_jni_headers/MenuModelBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;

namespace ui {
MenuModelBridge::MenuModelBridge(base::WeakPtr<ui::MenuModel> menu_model)
    : menu_model_(std::move(menu_model)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_ =
      Java_MenuModelBridge_create(env, reinterpret_cast<intptr_t>(this));
  AddExtensionItems();
}

MenuModelBridge::~MenuModelBridge() {
  Java_MenuModelBridge_destroyNative(base::android::AttachCurrentThread(),
                                     java_obj_);
}

void MenuModelBridge::ActivatedAt(JNIEnv* env, size_t i) {
  if (!menu_model_) {
    return;
  }
  menu_model_->ActivatedAt(i);
}

ScopedJavaGlobalRef<jobject> MenuModelBridge::GetJavaObject() {
  return java_obj_;
}

// private
void MenuModelBridge::AddExtensionItems() {
  if (!menu_model_) {
    return;
  }
  base::ElapsedTimer timer;
  JNIEnv* env = base::android::AttachCurrentThread();
  for (size_t i = 0; i < menu_model_->GetItemCount(); ++i) {
    if (!menu_model_->IsVisibleAt(i)) {
      continue;
    }
    switch (menu_model_->GetTypeAt(i)) {
      case MenuModel::TYPE_COMMAND:
      case MenuModel::TYPE_HIGHLIGHTED: {
        /* Translate both of these to basic items for now. */
        ImageModel image = menu_model_->GetIconAt(i);
        std::optional<SkBitmap> optional_bitmap;
        if (image.IsImage()) {
          optional_bitmap = image.GetImage().AsBitmap();
        }
        Java_MenuModelBridge_addCommand(
            env, java_obj_, menu_model_->GetLabelAt(i), optional_bitmap,
            menu_model_->IsEnabledAt(i), i);
        break;
      }
      case MenuModel::TYPE_CHECK:
        Java_MenuModelBridge_addCheck(
            env, java_obj_, menu_model_->GetLabelAt(i),
            menu_model_->IsItemCheckedAt(i), menu_model_->IsEnabledAt(i), i);
        break;
      case MenuModel::TYPE_RADIO:
        Java_MenuModelBridge_addRadioButton(
            env, java_obj_, menu_model_->GetLabelAt(i),
            menu_model_->IsItemCheckedAt(i), menu_model_->IsEnabledAt(i), i);
        break;
      case MenuModel::TYPE_SEPARATOR: {
        Java_MenuModelBridge_addDivider(env, java_obj_);
        break;
      }
      /* Don't handle TYPE_BUTTON_ITEM for now; it's not available in the Chrome
       * extensions API. */
      case MenuModel::TYPE_SUBMENU: {
        ImageModel image = menu_model_->GetIconAt(i);
        std::optional<SkBitmap> optional_bitmap;
        if (image.IsImage()) {
          optional_bitmap = image.GetImage().AsBitmap();
        }
        auto submenu_model_bridge = std::make_unique<MenuModelBridge>(
            menu_model_->GetSubmenuModelAt(i)->AsWeakPtr());
        Java_MenuModelBridge_addSubmenu(
            env, java_obj_, menu_model_->GetLabelAt(i), optional_bitmap,
            menu_model_->IsEnabledAt(i), submenu_model_bridge->GetJavaObject());
        submenu_model_bridges_.push_back(std::move(submenu_model_bridge));
        break;
      }
      /* Don't handle TYPE_ACTIONABLE_SUBMENU for now; it's not available in the
       * Chrome extensions API. */
      case MenuModel::TYPE_TITLE:
        // TODO(jhimawan): Call Java MenuModelBridge to add title item.
        break;
      default:
        break;
        /* Do nothing. */
    }
  }

  base::UmaHistogramCustomMicrosecondsTimes(
      "MenuModelBridge.AddExtensionItems.Duration",
      base::Microseconds(timer.Elapsed().InMicrosecondsF()),
      base::Microseconds(1), base::Microseconds(2000), 100);
}

}  // namespace ui

DEFINE_JNI(MenuModelBridge)
