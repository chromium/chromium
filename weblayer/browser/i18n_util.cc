// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/i18n_util.h"

#include "base/i18n/rtl.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "net/http/http_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/locale_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "weblayer/browser/java/jni/LocaleChangedBroadcastReceiver_jni.h"
#endif

namespace {

base::RepeatingClosureList& GetLocaleChangeClosures() {
  static base::NoDestructor<base::RepeatingClosureList> instance;
  return *instance;
}

}  // namespace

namespace weblayer {
namespace i18n {

std::string GetApplicationLocale() {
  // The locale is set in ContentMainDelegateImpl::InitializeResourceBundle().
  return base::i18n::GetConfiguredLocale();
}

std::string GetAcceptLangs() {
#if BUILDFLAG(IS_ANDROID)
  std::string locale_list = base::android::GetDefaultLocaleListString();
#else
  std::string locale_list = GetApplicationLocale();
#endif
  return net::HttpUtil::ExpandLanguageList(locale_list);
}

base::CallbackListSubscription RegisterLocaleChangeCallback(
    base::RepeatingClosure locale_changed) {
  return GetLocaleChangeClosures().Add(locale_changed);
}

#if BUILDFLAG(IS_ANDROID)
static void JNI_LocaleChangedBroadcastReceiver_LocaleChanged(JNIEnv* env) {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce([]() {
        // Passing an empty |pref_locale| means the Android system locale will
        // be used (base::android::GetDefaultLocaleString()).
        ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(
            {} /*pref_locale*/);
      }),
      base::BindOnce([]() { GetLocaleChangeClosures().Notify(); }));
  // TODO(estade): need to update the ResourceBundle for non-Browser processes
  // as well.
}
#endif

}  // namespace i18n
}  // namespace weblayer
