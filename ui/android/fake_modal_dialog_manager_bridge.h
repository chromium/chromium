// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_FAKE_MODAL_DIALOG_MANAGER_BRIDGE_H_
#define UI_ANDROID_FAKE_MODAL_DIALOG_MANAGER_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "ui/android/modal_dialog_manager_bridge.h"
#include "ui/android/ui_android_export.h"

namespace ui {

class WindowAndroid;

class UI_ANDROID_EXPORT FakeModalDialogManagerBridge {
 public:
  // `use_empty_java_presenter`, when set to true, tells
  // `FakeModalDialogManager.java` to use an empty Presenter instead of a mocked
  // one. An empty presenter is typically for browser tests and a mocked one is
  // for unittests.
  static std::unique_ptr<FakeModalDialogManagerBridge> CreateForTab(
      WindowAndroid* window,
      bool use_empty_java_presenter);

  FakeModalDialogManagerBridge(const FakeModalDialogManagerBridge&) = delete;
  FakeModalDialogManagerBridge& operator=(const FakeModalDialogManagerBridge&) =
      delete;
  ~FakeModalDialogManagerBridge();

  void ClickPositiveButton();

  bool IsSuspend(ModalDialogManagerBridge::ModalDialogType dialog_type);

 private:
  FakeModalDialogManagerBridge(
      base::android::ScopedJavaLocalRef<jobject> j_fake_manager_,
      WindowAndroid* window);

  const base::android::ScopedJavaLocalRef<jobject> j_fake_manager_;
  const raw_ptr<WindowAndroid> window_;
};

}  // namespace ui

#endif  // UI_ANDROID_FAKE_MODAL_DIALOG_MANAGER_BRIDGE_H_
