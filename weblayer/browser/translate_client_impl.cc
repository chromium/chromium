// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/translate_client_impl.h"

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/language/core/browser/pref_names.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/content/browser/content_translate_util.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/variations/service/variations_service.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "weblayer/browser/accept_languages_service_factory.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/page_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/browser/translate_ranker_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/infobars/content/content_infobar_manager.h"
#include "weblayer/browser/translate_compact_infobar.h"
#endif

namespace weblayer {

namespace {

std::unique_ptr<translate::TranslatePrefs> CreateTranslatePrefs(
    PrefService* prefs) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs(
      new translate::TranslatePrefs(prefs));

  // We need to obtain the country here, since it comes from VariationsService.
  // components/ does not have access to that.
  variations::VariationsService* variations_service =
      FeatureListCreator::GetInstance()->variations_service();
  if (variations_service) {
    translate_prefs->SetCountry(
        variations_service->GetStoredPermanentCountry());
  }

  return translate_prefs;
}

}  // namespace

TranslateClientImpl::TranslateClientImpl(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TranslateClientImpl>(*web_contents),
      translate_driver_(*web_contents,
                        /*url_language_histogram=*/nullptr,
                        /*translate_model_service=*/nullptr),
      translate_manager_(new translate::TranslateManager(
          this,
          TranslateRankerFactory::GetForBrowserContext(
              web_contents->GetBrowserContext()),
          /*language_model=*/nullptr)) {
  observation_.Observe(&translate_driver_);
  translate_driver_.set_translate_manager(translate_manager_.get());
}

TranslateClientImpl::~TranslateClientImpl() = default;

const translate::LanguageState& TranslateClientImpl::GetLanguageState() {
  return *translate_manager_->GetLanguageState();
}

bool TranslateClientImpl::ShowTranslateUI(translate::TranslateStep step,
                                          const std::string& source_language,
                                          const std::string& target_language,
                                          translate::TranslateErrors error_type,
                                          bool triggered_from_menu) {
#if BUILDFLAG(IS_ANDROID)
  if (error_type != translate::TranslateErrors::NONE)
    step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;
  translate::TranslateInfoBarDelegate::Create(
      step != translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
      translate_manager_->GetWeakPtr(),
      infobars::ContentInfoBarManager::FromWebContents(web_contents()), step,
      source_language, target_language, error_type, triggered_from_menu);
  return true;
#else
  return false;
#endif
}

translate::TranslateDriver* TranslateClientImpl::GetTranslateDriver() {
  return &translate_driver_;
}

translate::TranslateManager* TranslateClientImpl::GetTranslateManager() {
  return translate_manager_.get();
}

PrefService* TranslateClientImpl::GetPrefs() {
  BrowserContextImpl* browser_context =
      static_cast<BrowserContextImpl*>(web_contents()->GetBrowserContext());
  return browser_context->pref_service();
}

std::unique_ptr<translate::TranslatePrefs>
TranslateClientImpl::GetTranslatePrefs() {
  return CreateTranslatePrefs(GetPrefs());
}

language::AcceptLanguagesService*
TranslateClientImpl::GetAcceptLanguagesService() {
  return AcceptLanguagesServiceFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<infobars::InfoBar> TranslateClientImpl::CreateInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  return std::make_unique<TranslateCompactInfoBar>(std::move(delegate));
}

int TranslateClientImpl::GetInfobarIconID() const {
  NOTREACHED();
  return 0;
}
#endif

bool TranslateClientImpl::IsTranslatableURL(const GURL& url) {
  return translate::IsTranslatableURL(url);
}

void TranslateClientImpl::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  // Inform NavigationControllerImpl that the language has been determined. Note
  // that this event is implicitly regarded as being for the Page corresponding
  // to the most recently committed primary main-frame navigation, if one exists
  // (see the call to SetPageLanguageInNavigation() in
  // ContentTranslateDriver::RegisterPage()); this corresponds to
  // WebContents::GetPrimaryMainFrame()::GetPage(). Note also that in certain
  // corner cases (e.g., tab startup) there might not be such a committed
  // primary main-frame navigation; in those cases there won't be a
  // weblayer::Page corresponding to the primary page, as weblayer::Page objects
  // are created only at navigation commit.
  // TODO(crbug.com/1231889): Rearchitect translate's renderer-browser Mojo
  // connection to be able to explicitly determine the document/content::Page
  // with which this language determination event is associated.
  PageImpl* page =
      PageImpl::GetForPage(web_contents()->GetPrimaryMainFrame()->GetPage());
  if (page) {
    std::string language = details.adopted_language;

    auto* tab = TabImpl::FromWebContents(web_contents());
    auto* navigation_controller =
        static_cast<NavigationControllerImpl*>(tab->GetNavigationController());
    navigation_controller->OnPageLanguageDetermined(page, language);
  }

  // Show translate UI if desired.
  if (show_translate_ui_on_ready_) {
    GetTranslateManager()->ShowTranslateUI();
    show_translate_ui_on_ready_ = false;
  }
}

void TranslateClientImpl::ShowTranslateUiWhenReady() {
  if (GetLanguageState().source_language().empty()) {
    show_translate_ui_on_ready_ = true;
  } else {
    GetTranslateManager()->ShowTranslateUI();
  }
}

void TranslateClientImpl::WebContentsDestroyed() {
  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with web_contents() being null.
  translate_manager_.reset();
}

}  // namespace weblayer

WEB_CONTENTS_USER_DATA_KEY_IMPL(weblayer::TranslateClientImpl);
