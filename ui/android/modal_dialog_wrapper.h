// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_MODAL_DIALOG_WRAPPER_H_
#define UI_ANDROID_MODAL_DIALOG_WRAPPER_H_

#include <optional>
#include <vector>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/android/ui_android_export.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/dialog_model_host.h"

namespace ui {
class DialogModel;
class DialogModelMenuItem;
class WindowAndroid;
}  // namespace ui

namespace ui {

// Allows dialogs defined by ui::DialogModel() to be shown in android.
// This makes a best effort to map between ui::DialogModel() and Java
// PropertyModel.
//
// Maps title, an optional single paragraph, and ok and cancel buttons.
// Default labels for IDS_APP_OK and IDS_APP_CANCEL will be used for buttons
// if labels are not specified.
// Replacements (if any) are performed in paragraph text, but any emphasis is
// not included since it is not supported in android dialogs.
class UI_ANDROID_EXPORT ModalDialogWrapper : public DialogModelHost,
                                             public DialogModelFieldHost {
 public:
  // Mirrors Java's `ModalDialogProperties#ButtonStyles`.
  // TODO(crbug.com/392977703): IntDef that enum in C++.
  enum class ModalDialogButtonStyles {
    kPrimaryOutlineNegativeOutline = 0,
    kPrimaryFilledNegativeOutline = 1,
    kPrimaryOutlineNegativeFilled = 2,
    kPrimaryFilledNoNegative = 3,
  };

  static void ShowTabModal(std::unique_ptr<ui::DialogModel> dialog_model,
                           ui::WindowAndroid* window);

  static ModalDialogWrapper* GetDialogForTesting();

  ModalDialogWrapper(const ModalDialogWrapper&) = delete;
  ModalDialogWrapper& operator=(const ModalDialogWrapper&) = delete;
  virtual ~ModalDialogWrapper();

  // JNI methods.
  void PositiveButtonClicked(JNIEnv* env);
  void NegativeButtonClicked(JNIEnv* env);
  void CheckboxToggled(JNIEnv* env, bool is_checked);
  void MenuItemClicked(JNIEnv* env, int32_t index);
  void ParagraphLinkClicked(JNIEnv* env, int32_t index);
  void Dismissed(JNIEnv* env, int32_t dismissalCause);
  void Destroy(JNIEnv* env);

  // LINT.IfChange(DismissalCause)
  // Corresponds to the DialogDismissalCause in DialogDismissalCause.java.
  enum class DismissalCause {
    UNKNOWN = 0,
    POSITIVE_BUTTON_CLICKED = 1,
    NEGATIVE_BUTTON_CLICKED = 2,
    ACTION_ON_CONTENT = 3,
    DISMISSED_BY_NATIVE = 4,
    NAVIGATE_BACK = 5,
    TOUCH_OUTSIDE = 6,
    NAVIGATE_BACK_OR_TOUCH_OUTSIDE = 7,
    TAB_SWITCHED = 8,
    TAB_DESTROYED = 9,
    ACTIVITY_DESTROYED = 10,
    NOT_ATTACHED_TO_WINDOW = 11,
    NAVIGATE = 12,
    WEB_CONTENTS_DESTROYED = 13,
    DIALOG_INTERACTION_DEFERRED = 14,
    ACTION_ON_DIALOG_COMPLETED = 15,
    ACTION_ON_DIALOG_NOT_POSSIBLE = 16,
    CLIENT_TIMEOUT = 17,
  };
  // LINT.ThenChange(//ui/android/java/src/org/chromium/ui/modaldialog/DialogDismissalCause.java)

  std::optional<DismissalCause> GetDismissalCause() const {
    if (!dismissal_cause_) {
      return std::nullopt;
    }
    return static_cast<DismissalCause>(*dismissal_cause_);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ModalDialogWrapperTest, CloseDialogFromNative);
  FRIEND_TEST_ALL_PREFIXES(ModalDialogWrapperTest, DismissalCause_NativeClose);
  FRIEND_TEST_ALL_PREFIXES(ModalDialogWrapperTest,
                           MenuItem_CallbackDismissesDialog);
  FRIEND_TEST_ALL_PREFIXES(ModalDialogWrapperTest,
                           NoCrashOnJavaDismissAfterNativeDestroy);

  ModalDialogWrapper(std::unique_ptr<ui::DialogModel> dialog_model,
                     ui::WindowAndroid* window_android);

  // `DialogModelHost`:
  void Close() override;
  void OnDialogButtonChanged() override;

  // Helper function for BuildPropertyModel.
  ModalDialogButtonStyles GetButtonStyles() const;

  // Build java PropertyModel from ui::DialogModel.
  void BuildPropertyModel();

  const std::unique_ptr<ui::DialogModel> dialog_model_;

  std::optional<int> dismissal_cause_;

  ElementIdentifier checkbox_id_;
  std::vector<DialogModelMenuItem*> menu_items_;

  const raw_ptr<WindowAndroid> window_android_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace ui

#endif  // UI_ANDROID_MODAL_DIALOG_WRAPPER_H_
