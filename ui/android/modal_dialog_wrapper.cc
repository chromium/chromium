// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/modal_dialog_wrapper.h"

#include <jni.h>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "third_party/jni_zero/default_conversions.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/android/modal_dialog_manager_bridge.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/strings/grit/ui_strings.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/ModalDialogWrapper_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::JavaRef;
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
  dialog_model_->set_host(DialogModelHost::GetPassKey(), this);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_ = Java_ModalDialogWrapper_create(
      env, reinterpret_cast<uintptr_t>(this), window_android_->GetJavaObject());
}

ModalDialogWrapper::~ModalDialogWrapper() {
  dialog_model_->OnDialogDestroying(DialogModelHost::GetPassKey());
  Java_ModalDialogWrapper_clearNativePtr(base::android::AttachCurrentThread(),
                                         java_obj_);
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
    std::vector<std::u16string> replacement_texts;
    for (const auto& replacement : replacements) {
      replacement_texts.push_back(replacement.text());
    }

    // Find the offsets of the replacements in the localized string.
    std::vector<size_t> offsets;
    const std::u16string final_text = l10n_util::GetStringFUTF16(
        label.message_id(), replacement_texts, &offsets);

    // Slice the final text into spans based on the offsets of the replacements.
    // This allows Java to build a SpannableString where only the replacement
    // portions are clickable/styled.
    // Note: This assumes replacements appear in numerical order ($1 before $2).
    size_t current_pos = 0;
    for (size_t i = 0; i < offsets.size(); ++i) {
      size_t start = offsets[i];
      // Capture the plain text preceding the replacement.
      if (start > current_pos) {
        data.spans.push_back(
            final_text.substr(current_pos, start - current_pos));
        data.closures.emplace_back();
      }

      // Handle the current replacement.
      const auto& replacement = replacements[i];
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
        data.closures.emplace_back();
      }
      current_pos = start + replacement.text().length();
    }

    // Capture trailing text after all replacements.
    if (current_pos < final_text.length()) {
      data.spans.push_back(final_text.substr(current_pos));
      data.closures.emplace_back();
    }
  }
  return data;
}

SkBitmap getIconBitmap(const ui::ImageModel& icon_model, float scale) {
  if (icon_model.IsEmpty()) {
    return SkBitmap();
  }

  auto key = ui::ColorProviderKey();
  ui::ColorProvider* color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(key);
  CHECK(color_provider);

  gfx::ImageSkia image_skia = icon_model.Rasterize(color_provider);
  if (image_skia.isNull()) {
    return SkBitmap();
  }

  return image_skia.GetRepresentation(scale).GetBitmap();
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
      ok_button->style().value_or(ButtonStyle::kProminent);
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

float GetScaleFactor(WindowAndroid* window) {
  display::Screen* screen = display::Screen::Get();
  if (!screen || !window) {
    return 1.0f;
  }
  return screen->GetPreferredScaleFactorForWindow(window).value_or(1.0f);
}

void ModalDialogWrapper::BuildPropertyModel() {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> ok_button_label = GetButtonLabel(
      env, dialog_model_->ok_button(DialogModelHost::GetPassKey()), IDS_APP_OK);
  ScopedJavaLocalRef<jstring> cancel_button_label = GetButtonLabel(
      env, dialog_model_->cancel_button(DialogModelHost::GetPassKey()),
      IDS_APP_CANCEL);

  ModalDialogButtonStyles buttonStyles = GetButtonStyles();

  Java_ModalDialogWrapper_withTitleAndButtons(
      env, java_obj_, dialog_model_->title(DialogModelHost::GetPassKey()),
      ok_button_label, cancel_button_label, static_cast<int>(buttonStyles));

  float scale = GetScaleFactor(window_android_);
  SkBitmap bitmap =
      getIconBitmap(dialog_model_->icon(DialogModelHost::GetPassKey()), scale);
  if (!bitmap.isNull()) {
    Java_ModalDialogWrapper_withTitleIcon(env, java_obj_, bitmap);
  }

  std::u16string checkbox_text;
  bool checked = false;
  std::vector<std::vector<std::u16string>> all_paragraph_spans;
  std::vector<std::vector<base::RepeatingClosure>> all_paragraph_closures;
  std::vector<SkBitmap> menu_item_icons;
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
        SkBitmap icon_bitmap = getIconBitmap(menu_item->icon(), scale);
        // Menu items without icons are not yet handled on Android.
        if (!icon_bitmap.isNull()) {
          menu_item_icons.push_back(std::move(icon_bitmap));
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
    ScopedJavaLocalRef<jobjectArray> first_spans_array = jni_zero::ToJniArray(
        env, all_paragraph_spans[0], jni_zero::g_string_class);
    jclass string_array_class = env->GetObjectClass(first_spans_array.obj());
    ScopedJavaLocalRef<jobjectArray> java_spans_array =
        jni_zero::NewArray<jobjectArray>(env, all_paragraph_spans.size(),
                                         string_array_class);
    java_spans_array.Set(env, 0, first_spans_array);

    for (size_t i = 1; i < all_paragraph_spans.size(); ++i) {
      ScopedJavaLocalRef<jobjectArray> spans_array = jni_zero::ToJniArray(
          env, all_paragraph_spans[i], jni_zero::g_string_class);
      java_spans_array.Set(env, i, spans_array);
    }

    jclass jni_callback_class =
        org_chromium_base_JniRepeatingCallback_clazz(env);
    ScopedJavaLocalRef<jobjectArray> first_callbacks_array =
        jni_zero::ToJniArray(env, all_paragraph_closures[0],
                             jni_callback_class);
    jclass callback_array_class =
        env->GetObjectClass(first_callbacks_array.obj());
    ScopedJavaLocalRef<jobjectArray> java_callbacks_array =
        jni_zero::NewArray<jobjectArray>(env, all_paragraph_closures.size(),
                                         callback_array_class);
    java_callbacks_array.Set(env, 0, first_callbacks_array);

    for (size_t i = 1; i < all_paragraph_closures.size(); ++i) {
      ScopedJavaLocalRef<jobjectArray> callbacks_array = jni_zero::ToJniArray(
          env, all_paragraph_closures[i], jni_callback_class);
      java_callbacks_array.Set(env, i, callbacks_array);
    }

    Java_ModalDialogWrapper_withMessageParagraphs(
        env, java_obj_, java_spans_array, java_callbacks_array);
  }

  if (!checkbox_text.empty()) {
    Java_ModalDialogWrapper_withCheckbox(env, java_obj_, checkbox_text,
                                         checked);
  }

  if (!menu_item_icons.empty()) {
    Java_ModalDialogWrapper_withMenuItems(env, java_obj_, menu_item_icons,
                                          menu_item_labels);
  }
}

void ModalDialogWrapper::PositiveButtonClicked(JNIEnv* env) {
  dialog_model_->OnDialogAcceptAction(DialogModelHost::GetPassKey());
}

void ModalDialogWrapper::NegativeButtonClicked(JNIEnv* env) {
  dialog_model_->OnDialogCancelAction(DialogModelHost::GetPassKey());
}

void ModalDialogWrapper::CheckboxToggled(JNIEnv* env, bool is_checked) {
  if (!checkbox_id_) {
    return;
  }
  dialog_model_->GetCheckboxByUniqueId(checkbox_id_)
      ->OnChecked(DialogModelFieldHost::GetPassKey(), is_checked);
}

void ModalDialogWrapper::MenuItemClicked(JNIEnv* env, int32_t index) {
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), menu_items_.size());
  menu_items_[index]->OnActivated(DialogModelFieldHost::GetPassKey(), 0);
}

void ModalDialogWrapper::Dismissed(JNIEnv* env, int32_t dismissalCause) {
  dismissal_cause_ = dismissalCause;
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
