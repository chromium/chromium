// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/translate_compact_infobar.h"

#include <stddef.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/translate/content/android/translate_utils.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/java/jni/TranslateCompactInfoBar_jni.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/browser/translate_client_impl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

// Finch parameter names:
const char kTranslateTabDefaultTextColor[] = "translate_tab_default_text_color";

// TranslateInfoBar -----------------------------------------------------------

TranslateCompactInfoBar::TranslateCompactInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate)
    : infobars::InfoBarAndroid(std::move(delegate)), action_flags_(FLAG_NONE) {
  GetDelegate()->AddObserver(this);

  // Flip the translate bit if auto translate is enabled.
  if (GetDelegate()->translate_step() == translate::TRANSLATE_STEP_TRANSLATING)
    action_flags_ |= FLAG_TRANSLATE;
}

TranslateCompactInfoBar::~TranslateCompactInfoBar() {
  GetDelegate()->RemoveObserver(this);
}

ScopedJavaLocalRef<jobject> TranslateCompactInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();

  translate::JavaLanguageInfoWrapper translate_languages =
      translate::TranslateUtils::GetTranslateLanguagesInJavaFormat(env,
                                                                   delegate);

  ScopedJavaLocalRef<jstring> source_language_code =
      base::android::ConvertUTF8ToJavaString(env,
                                             delegate->source_language_code());

  ScopedJavaLocalRef<jstring> target_language_code =
      base::android::ConvertUTF8ToJavaString(env,
                                             delegate->target_language_code());
  content::WebContents* web_contents =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(this);

  TabImpl* tab =
      web_contents ? TabImpl::FromWebContents(web_contents) : nullptr;

  return Java_TranslateCompactInfoBar_create(
      env, tab ? tab->GetJavaTab() : nullptr, delegate->translate_step(),
      source_language_code, target_language_code,
      delegate->ShouldNeverTranslateLanguage(),
      delegate->IsSiteOnNeverPromptList(), delegate->ShouldAlwaysTranslate(),
      delegate->triggered_from_menu(), translate_languages.java_languages,
      translate_languages.java_codes, translate_languages.java_hash_codes,
      TabDefaultTextColor());
}

void TranslateCompactInfoBar::ProcessButton(int action) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  if (action == infobars::InfoBarAndroid::ACTION_TRANSLATE) {
    action_flags_ |= FLAG_TRANSLATE;
    delegate->Translate();
    if (delegate->ShouldAutoAlwaysTranslate()) {
      JNIEnv* env = base::android::AttachCurrentThread();
      Java_TranslateCompactInfoBar_setAutoAlwaysTranslate(env,
                                                          GetJavaInfoBar());
    }
  } else if (action ==
             infobars::InfoBarAndroid::ACTION_TRANSLATE_SHOW_ORIGINAL) {
    action_flags_ |= FLAG_REVERT;
    delegate->RevertWithoutClosingInfobar();
  } else {
    DCHECK_EQ(infobars::InfoBarAndroid::ACTION_NONE, action);
  }
}

void TranslateCompactInfoBar::SetJavaInfoBar(
    const base::android::JavaRef<jobject>& java_info_bar) {
  infobars::InfoBarAndroid::SetJavaInfoBar(java_info_bar);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TranslateCompactInfoBar_setNativePtr(env, java_info_bar,
                                            reinterpret_cast<intptr_t>(this));
}

void TranslateCompactInfoBar::ApplyStringTranslateOption(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    int option,
    const JavaParamRef<jstring>& value) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  if (option == translate::TranslateUtils::OPTION_SOURCE_CODE) {
    std::string source_code =
        base::android::ConvertJavaStringToUTF8(env, value);
    delegate->UpdateSourceLanguage(source_code);
  } else if (option == translate::TranslateUtils::OPTION_TARGET_CODE) {
    std::string target_code =
        base::android::ConvertJavaStringToUTF8(env, value);
    delegate->UpdateTargetLanguage(target_code);
  } else {
    DCHECK(false);
  }
}

void TranslateCompactInfoBar::ApplyBoolTranslateOption(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    int option,
    jboolean value) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  if (option == translate::TranslateUtils::OPTION_ALWAYS_TRANSLATE) {
    if (delegate->ShouldAlwaysTranslate() != value) {
      action_flags_ |= FLAG_ALWAYS_TRANSLATE;
      delegate->ToggleAlwaysTranslate();
    }
  } else if (option == translate::TranslateUtils::OPTION_NEVER_TRANSLATE) {
    bool language_blocklisted = !delegate->IsTranslatableLanguageByPrefs();
    if (language_blocklisted != value) {
      delegate->RevertWithoutClosingInfobar();
      action_flags_ |= FLAG_NEVER_LANGUAGE;
      delegate->ToggleTranslatableLanguageByPrefs();
    }
  } else if (option == translate::TranslateUtils::OPTION_NEVER_TRANSLATE_SITE) {
    if (delegate->IsSiteOnNeverPromptList() != value) {
      delegate->RevertWithoutClosingInfobar();
      action_flags_ |= FLAG_NEVER_SITE;
      delegate->ToggleNeverPromptSite();
    }
  } else {
    DCHECK(false);
  }
}

jboolean TranslateCompactInfoBar::ShouldAutoNeverTranslate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean menu_expanded) {
  // Flip menu expanded bit.
  if (menu_expanded)
    action_flags_ |= FLAG_EXPAND_MENU;

  if (!IsDeclinedByUser())
    return false;

  return GetDelegate()->ShouldAutoNeverTranslate();
}

// Returns true if the current tab is an incognito tab.
jboolean TranslateCompactInfoBar::IsIncognito(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  content::WebContents* web_contents =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(this);
  if (!web_contents)
    return false;
  return web_contents->GetBrowserContext()->IsOffTheRecord();
}

int TranslateCompactInfoBar::GetParam(const std::string& paramName,
                                      int default_value) {
  std::map<std::string, std::string> params;
  if (!base::GetFieldTrialParams(translate::kTranslateCompactUI.name,
                                 &params)) {
    return default_value;
  }
  int value = 0;
  base::StringToInt(params[paramName], &value);
  return value <= 0 ? default_value : value;
}

int TranslateCompactInfoBar::TabDefaultTextColor() {
  return GetParam(kTranslateTabDefaultTextColor, 0);
}

translate::TranslateInfoBarDelegate* TranslateCompactInfoBar::GetDelegate() {
  return delegate()->AsTranslateInfoBarDelegate();
}

void TranslateCompactInfoBar::OnTranslateStepChanged(
    translate::TranslateStep step,
    translate::TranslateErrors error_type) {
  // If the tab lost active state while translation was occurring, the Java
  // infobar will now be gone. In that case there is nothing to do here.
  if (!HasSetJavaInfoBar())
    return;  // No connected Java infobar

  if (!owner())
    return;  // We're closing; don't call anything.

  if ((step == translate::TRANSLATE_STEP_AFTER_TRANSLATE) ||
      (step == translate::TRANSLATE_STEP_TRANSLATE_ERROR)) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_TranslateCompactInfoBar_onPageTranslated(
        env, GetJavaInfoBar(), base::to_underlying(error_type));
  }
}

void TranslateCompactInfoBar::OnTargetLanguageChanged(
    const std::string& target_language_code) {
  // In WebLayer, target language changes are only initiated by the UI. This
  // method should always be a no-op.
  DCHECK_EQ(GetDelegate()->target_language_code(), target_language_code);
}

bool TranslateCompactInfoBar::IsDeclinedByUser() {
  // Whether there is any affirmative action bit.
  return action_flags_ == FLAG_NONE;
}

void TranslateCompactInfoBar::OnTranslateInfoBarDelegateDestroyed(
    translate::TranslateInfoBarDelegate* delegate) {
  DCHECK_EQ(GetDelegate(), delegate);
  GetDelegate()->RemoveObserver(this);
}

}  // namespace weblayer
