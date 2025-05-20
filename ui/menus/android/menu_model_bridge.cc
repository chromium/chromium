// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/menus/android/menu_model_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_callback.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/menu_model_bridge_jni_headers/MenuModelBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJniCallback;

namespace ui {
MenuModelBridge::MenuModelBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_ = Java_MenuModelBridge_create(env);
}

MenuModelBridge::~MenuModelBridge() = default;

void MenuModelBridge::AddExtensionItems(ui::MenuModel* menu_model) {
  JNIEnv* env = base::android::AttachCurrentThread();
  for (size_t i = 0; i < menu_model->GetItemCount(); ++i) {
    if (!menu_model->IsVisibleAt(i)) {
      continue;
    }
    switch (menu_model->GetTypeAt(i)) {
      case MenuModel::TYPE_COMMAND:
      case MenuModel::TYPE_HIGHLIGHTED: {
        /* Translate both of these to basic items for now. */
        Java_MenuModelBridge_addCommand(
            env, java_obj_, menu_model->GetLabelAt(i),
            menu_model->GetIconAt(i).GetImage().AsBitmap(),
            menu_model->IsEnabledAt(i),
            ToJniCallback(
                env,
                base::BindOnce(
                    &MenuModelBridge::ActivatedAt,
                    /* If the weak pointer to "this" becomes
                       invalidated, the method will not be called, see
                       https://chromium.googlesource.com/chromium/src.git/+/master/docs/callback.md#Binding-A-Class-Method-With-Weak-Pointers
                     */
                    weak_ptr_factory_.GetWeakPtr(), menu_model->AsWeakPtr(),
                    i)));
        break;
      }
      case MenuModel::TYPE_CHECK:
        Java_MenuModelBridge_addCheck(
            env, java_obj_, menu_model->GetLabelAt(i),
            menu_model->IsItemCheckedAt(i), menu_model->IsEnabledAt(i),
            ToJniCallback(env, base::BindOnce(&MenuModelBridge::ActivatedAt,
                                              weak_ptr_factory_.GetWeakPtr(),
                                              menu_model->AsWeakPtr(), i)));
        break;
      case MenuModel::TYPE_RADIO:
        Java_MenuModelBridge_addRadioButton(
            env, java_obj_, menu_model->GetLabelAt(i),
            menu_model->IsItemCheckedAt(i), menu_model->IsEnabledAt(i),
            ToJniCallback(env, base::BindOnce(&MenuModelBridge::ActivatedAt,
                                              weak_ptr_factory_.GetWeakPtr(),
                                              menu_model->AsWeakPtr(), i)));
        break;
      case MenuModel::TYPE_SEPARATOR: {
        Java_MenuModelBridge_addDivider(env, java_obj_);
        break;
      }
      /* Don't handle TYPE_BUTTON_ITEM for now; it's not available in the Chrome
       * extensions API. */
      case MenuModel::TYPE_SUBMENU:
        // TODO(jhimawan): Call Java MenuModelBridge to add submenu item.
        break;
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
}

// private
void MenuModelBridge::ActivatedAt(base::WeakPtr<MenuModel> menu_model_weak_ptr,
                                  size_t i) {
  /* Should always null-check WeakPtr, see
   * https://source.chromium.org/chromium/chromium/src/+/main:base/memory/weak_ptr.h;l=192;drc=192000bedcb9891ac5de22e98115f2578389e1e5
   */
  if (!menu_model_weak_ptr) {
    return;
  }
  menu_model_weak_ptr->ActivatedAt(i);
}

}  // namespace ui
