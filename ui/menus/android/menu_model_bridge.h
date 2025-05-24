// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MENUS_ANDROID_MENU_MODEL_BRIDGE_H_
#define UI_MENUS_ANDROID_MENU_MODEL_BRIDGE_H_

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/models/menu_model.h"

// Used to translate
// https://source.chromium.org/chromium/chromium/src/+/main:ui/base/models/menu_model.h;l=168
// to Java List<PropertyModel>. See
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/contextmenu/MenuModelBridge.java

namespace ui {
class MenuModelBridge {
 public:
  MenuModelBridge();
  MenuModelBridge(const MenuModelBridge&) = delete;
  MenuModelBridge& operator=(const MenuModelBridge&) = delete;
  virtual ~MenuModelBridge();

  void AddExtensionItems(ui::MenuModel* menu_model);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  base::WeakPtrFactory<MenuModelBridge> weak_ptr_factory_{this};

  void ActivatedAt(base::WeakPtr<ui::MenuModel> menu_model, size_t i);
};

}  // namespace ui

#endif  // UI_MENUS_ANDROID_MENU_MODEL_BRIDGE_H_
