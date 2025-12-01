// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/modal_dialog_wrapper.h"

#include <jni.h>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_callback.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "ui/android/modal_dialog_manager_bridge.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/strings/grit/ui_strings.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/ModalDialogWrapper_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace ui {

ModalDialogWrapper* g_dialog_ptr_for_testing = nullptr;

// static:
void ModalDialogWrapper::ShowTabModal(
    std::unique_ptr<ui::DialogModel> dialog_model,
    ui::WindowAndroid* window) {
  ModalDialogWrapper* tab_modal =
      new ModalDialogWrapper(std::move(dialog_model), window);
  g_dialog_ptr_for_testing = tab_modal;
  tab_modal->BuildPropertyModel();
  auto* dialog_manager = window->GetModalDialogManagerBridge();
  CHECK(dialog_manager);
  dialog_manager->ShowDialog(tab_modal->java_obj_,
                             ModalDialogManagerBridge::ModalDialogType::kTab);
  // `tab_modal` will delete itself when dialog is dismissed.
}

// static:
ModalDialogWrapper* ModalDialogWrapper::GetDialogForTesting() {
  return g_dialog_ptr_for_testing;
}

// private:
ModalDialogWrapper::ModalDialogWrapper(
    std::unique_ptr<DialogModel> dialog_model,
    ui::WindowAndroid* window_android)
    : dialog_model_(std::move(dialog_model)), window_android_(window_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_ = Java_ModalDialogWrapper_create(
      env, reinterpret_cast<uintptr_t>(this), window_android_->GetJavaObject());
}

ModalDialogWrapper::~ModalDialogWrapper() {
  dialog_model_->OnDialogDestroying(DialogModelHost::GetPassKey());
}

namespace {  // private helper for ModalDialogWrapper::BuildPropertyModel

ScopedJavaLocalRef<jstring> GetButtonLabel(JNIEnv* env,
                                           DialogModel::Button* button,
                                           int default_label_id) {
  if (!button) {
    return ScopedJavaLocalRef<jstring>();
  }
  const std::u16string& label_text = button->label();
  return ConvertUTF16ToJavaString(
      env, label_text.empty() ? l10n_util::GetStringUTF16(default_label_id)
                              : label_text);
}

// A helper struct to hold the deconstructed data for a single paragraph.
struct ParagraphData {
  std::vector<std::u16string> spans;
  std::vector<base::RepeatingClosure> closures;
};

// Processes a paragraph field, deconstructing it into spans and closures.
ParagraphData getParagraphData(DialogModelField* field) {
  ParagraphData data;
  const DialogModelLabel& label = field->AsParagraph()->label();
  const auto& replacements = label.replacements();

  if (replacements.empty()) {
    data.spans.push_back(label.GetString());
    data.closures.emplace_back();  // Add a null closure.
  } else {
    for (const auto& replacement : replacements) {
      data.spans.push_back(replacement.text());
      if (replacement.callback().has_value()) {
        data.closures.push_back(base::BindRepeating(
            [](const DialogModelLabel::Callback& cb) {
              cb.Run(ui::TouchEvent(
                  ui::EventType::kTouchReleased, gfx::Point(),
                  ui::EventTimeForNow(),
                  ui::PointerDetails(ui::EventPointerType::kTouch), 0));
            },
            replacement.callback().value()));
      } else {
        data.closures.emplace_back();  // Add a null closure.
      }
    }
  }
  return data;
}

const SkBitmap* getIconBitmap(const ui::ImageModel& icon_model) {
  auto key = ui::ColorProviderKey();
  ui::ColorProvider* color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(key);
  CHECK(color_provider);

  gfx::ImageSkia image_skia = icon_model.Rasterize(color_provider);
  // Returns the 1x Skia if it exists. See ImageSkia.bitmap() for details.
  return image_skia.bitmap();
}

}  // anonymous namespace

ModalDialogWrapper::ModalDialogButtonStyles
ModalDialogWrapper::GetButtonStyles() const {
  auto* ok_button = dialog_model_->ok_button(DialogModelHost::GetPassKey());
  if (!ok_button) {
    return ModalDialogButtonStyles::kPrimaryOutlineNegativeOutline;
  }

  auto* cancel_button =
      dialog_model_->cancel_button(DialogModelHost::GetPassKey());

  const ButtonStyle ok_button_style =
      ok_button->style().value_or(ButtonStyle::kDefault);
  const ButtonStyle cancel_button_style =
      cancel_button ? cancel_button->style().value_or(ui::ButtonStyle::kDefault)
                    : ButtonStyle::kDefault;

  const std::optional<mojom::DialogButton>& override_default_button =
      dialog_model_->override_default_button(DialogModelHost::GetPassKey());

  const bool is_ok_prominent =
      (override_default_button == mojom::DialogButton::kOk) ||
      (ok_button_style == ui::ButtonStyle::kProminent &&
       !override_default_button.has_value());

  const bool is_cancel_prominent =
      (override_default_button == mojom::DialogButton::kCancel) ||
      (cancel_button_style == ui::ButtonStyle::kProminent &&
       !override_default_button.has_value());

  if (is_ok_prominent && is_cancel_prominent) {
    NOTREACHED() << "Both buttons cannot be prominent.";
  }

  if (is_ok_prominent) {
    return (cancel_button)
               ? ModalDialogButtonStyles::kPrimaryFilledNegativeOutline
               : ModalDialogButtonStyles::kPrimaryFilledNoNegative;
  }

  if (is_cancel_prominent) {
    return ModalDialogButtonStyles::kPrimaryOutlineNegativeFilled;
  }

  return ModalDialogButtonStyles::kPrimaryOutlineNegativeOutline;
}

void ModalDialogWrapper::BuildPropertyModel() {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> title = ConvertUTF16ToJavaString(
      env, dialog_model_->title(DialogModelHost::GetPassKey()));

  ScopedJavaLocalRef<jstring> ok_button_label = GetButtonLabel(
      env, dialog_model_->ok_button(DialogModelHost::GetPassKey()), IDS_APP_OK);
  ScopedJavaLocalRef<jstring> cancel_button_label = GetButtonLabel(
      env, dialog_model_->cancel_button(DialogModelHost::GetPassKey()),
      IDS_APP_CANCEL);

  ModalDialogButtonStyles buttonStyles = GetButtonStyles();

  Java_ModalDialogWrapper_withTitleAndButtons(
      env, java_obj_, title, ok_button_label, cancel_button_label,
      static_cast<int>(buttonStyles));

  const SkBitmap* bitmap =
      getIconBitmap(dialog_model_->icon(DialogModelHost::GetPassKey()));
  if (!bitmap->isNull()) {
    Java_ModalDialogWrapper_withTitleIcon(env, java_obj_,
                                          gfx::ConvertToJavaBitmap(*bitmap));
  }

  std::u16string checkbox_text;
  jboolean checked = false;
  std::vector<std::vector<std::u16string>> all_paragraph_spans;
  std::vector<std::vector<base::RepeatingClosure>> all_paragraph_closures;
  std::vector<const SkBitmap*> menu_item_icons;
  std::vector<std::u16string> menu_item_labels;
  menu_items_.clear();

  for (const auto& field :
       dialog_model_->fields(DialogModelHost::GetPassKey())) {
    switch (field->type()) {
      case DialogModelField::kParagraph: {
        ParagraphData paragraph_data = getParagraphData(field.get());
        all_paragraph_spans.push_back(std::move(paragraph_data.spans));
        all_paragraph_closures.push_back(std::move(paragraph_data.closures));
        break;
      }
      case DialogModelField::kCheckbox: {
        // TODO(crbug.com/428048190): A dialog should not have more than one
        // checkbox.
        CHECK(checkbox_text.empty())
            << "Dialogs with more than one checkbox are "
               "not yet supported on Android.";
        DialogModelCheckbox* checkbox_field = field->AsCheckbox();

        const DialogModelLabel& label = checkbox_field->label();
        // Checkboxes with replacements (links) are not yet supported on
        // Android.
        CHECK(label.replacements().empty());

        checkbox_text = label.GetString();
        checked = checkbox_field->is_checked();
        checkbox_id_ = checkbox_field->id();
        break;
      }
      case DialogModelField::kMenuItem: {
        DialogModelMenuItem* menu_item = field->AsMenuItem();
        const SkBitmap* icon_bitmap = getIconBitmap(menu_item->icon());
        // Menu items without icons are not yet handled on Android.
        if (icon_bitmap && !icon_bitmap->isNull()) {
          menu_item_icons.push_back(icon_bitmap);
          menu_item_labels.push_back(menu_item->label());
          menu_items_.push_back(menu_item);
        }
        break;
      }
      default:
        NOTREACHED() << "Unsupported DialogModel field type " << field->type()
                     << ". Support should be added before this dialog is used "
                        "in android";
    }
  }

  if (!all_paragraph_spans.empty()) {
    // vector<vector<u16string>> -> String[][]
    ScopedJavaLocalRef<jclass> string_array_class =
        base::android::GetClass(env, "[Ljava/lang/String;");
    auto java_spans_array = ScopedJavaLocalRef<jobjectArray>::Adopt(
        env, env->NewObjectArray(all_paragraph_spans.size(),
                                 string_array_class.obj(), nullptr));
    for (size_t i = 0; i < all_paragraph_spans.size(); ++i) {
      ScopedJavaLocalRef<jobjectArray> inner_array =
          base::android::ToJavaArrayOfStrings(env, all_paragraph_spans[i]);
      env->SetObjectArrayElement(java_spans_array.obj(), i, inner_array.obj());
    }

    // Create the 2D Java array for callbacks.
    ScopedJavaLocalRef<jclass> jni_callback_class =
        base::android::GetClass(env, "org/chromium/base/JniRepeatingCallback");

    // To get the class for an array of JniRepeatingCallback, we can create a
    // dummy array and get its class. This is necessary because the standard
    // "[Lorg/chromium/base/JniRepeatingCallback;" does not work.
    ScopedJavaLocalRef<jobjectArray> dummy_array =
        ScopedJavaLocalRef<jobjectArray>::Adopt(
            env, env->NewObjectArray(0, jni_callback_class.obj(), nullptr));
    CHECK(dummy_array);
    ScopedJavaLocalRef<jclass> jni_callback_array_class =
        ScopedJavaLocalRef<jclass>::Adopt(
            env, env->GetObjectClass(dummy_array.obj()));
    CHECK(jni_callback_array_class);

    auto java_callbacks_array = ScopedJavaLocalRef<jobjectArray>::Adopt(
        env, env->NewObjectArray(all_paragraph_closures.size(),
                                 jni_callback_array_class.obj(), nullptr));

    for (size_t i = 0; i < all_paragraph_closures.size(); ++i) {
      std::vector<ScopedJavaLocalRef<jobject>> jobjects;
      for (const auto& closure : all_paragraph_closures[i]) {
        if (closure) {
          jobjects.push_back(base::android::ToJniCallback(env, closure));
        } else {
          jobjects.push_back(nullptr);
        }
      }
      ScopedJavaLocalRef<jobjectArray> inner_array =
          base::android::ToJavaArrayOfObjects(env, jni_callback_class.obj(),
                                              jobjects);
      env->SetObjectArrayElement(java_callbacks_array.obj(), i,
                                 inner_array.obj());
    }

    Java_ModalDialogWrapper_withMessageParagraphs(
        env, java_obj_, java_spans_array, java_callbacks_array);
  }

  if (!checkbox_text.empty()) {
    ScopedJavaLocalRef<jstring> java_checkbox_label =
        ConvertUTF16ToJavaString(env, checkbox_text);
    Java_ModalDialogWrapper_withCheckbox(env, java_obj_, java_checkbox_label,
                                         checked);
  }

  if (!menu_item_icons.empty()) {
    ScopedJavaLocalRef<jclass> bitmap_class =
        base::android::GetClass(env, "android/graphics/Bitmap");
    auto java_icons_array = ScopedJavaLocalRef<jobjectArray>::Adopt(
        env, env->NewObjectArray(menu_item_icons.size(), bitmap_class.obj(),
                                 nullptr));
    for (size_t i = 0; i < menu_item_icons.size(); ++i) {
      env->SetObjectArrayElement(
          java_icons_array.obj(), i,
          gfx::ConvertToJavaBitmap(*menu_item_icons[i]).obj());
    }

    ScopedJavaLocalRef<jobjectArray> java_labels_array =
        base::android::ToJavaArrayOfStrings(env, menu_item_labels);

    Java_ModalDialogWrapper_withMenuItems(env, java_obj_, java_icons_array,
                                          java_labels_array);
  }
}

void ModalDialogWrapper::PositiveButtonClicked(JNIEnv* env) {
  dialog_model_->OnDialogAcceptAction(DialogModelHost::GetPassKey());
}

void ModalDialogWrapper::NegativeButtonClicked(JNIEnv* env) {
  dialog_model_->OnDialogCancelAction(DialogModelHost::GetPassKey());
}

void ModalDialogWrapper::CheckboxToggled(JNIEnv* env, jboolean is_checked) {
  if (!checkbox_id_) {
    return;
  }
  dialog_model_->GetCheckboxByUniqueId(checkbox_id_)
      ->OnChecked(DialogModelFieldHost::GetPassKey(),
                  static_cast<bool>(is_checked));
}

void ModalDialogWrapper::MenuItemClicked(JNIEnv* env, jint index) {
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), menu_items_.size());
  menu_items_[index]->OnActivated(DialogModelFieldHost::GetPassKey(), 0);
}

void ModalDialogWrapper::Dismissed(JNIEnv* env) {
  dialog_model_->OnDialogCloseAction(DialogModelHost::GetPassKey());
}

void ModalDialogWrapper::Destroy(JNIEnv* env) {
  delete this;
}

void ModalDialogWrapper::Close() {
  auto* dialog_manager = window_android_->GetModalDialogManagerBridge();
  CHECK(dialog_manager) << "The destruction of the ModalDialogManager.java "
                           "should also destroy this dialog wrapper.";
  dialog_manager->DismissDialog(java_obj_);
}

void ModalDialogWrapper::OnDialogButtonChanged() {}

}  // namespace ui

DEFINE_JNI(ModalDialogWrapper)
