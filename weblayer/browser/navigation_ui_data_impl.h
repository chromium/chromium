// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NAVIGATION_UI_DATA_IMPL_H_
#define WEBLAYER_BROWSER_NAVIGATION_UI_DATA_IMPL_H_

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_ui_data.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/embedder_support/android/util/web_resource_response.h"
#endif

namespace weblayer {

// Data that we pass to content::NavigationController::LoadURLWithParams
// and can access from content::NavigationHandle later.
class NavigationUIDataImpl : public content::NavigationUIData {
 public:
  NavigationUIDataImpl();
  NavigationUIDataImpl(const NavigationUIDataImpl&) = delete;
  NavigationUIDataImpl& operator=(const NavigationUIDataImpl&) = delete;
  ~NavigationUIDataImpl() override;

  // content::NavigationUIData implementation:
  std::unique_ptr<content::NavigationUIData> Clone() override;

  void set_disable_network_error_auto_reload(bool value) {
    disable_network_error_auto_reload_ = value;
  }
  bool disable_network_error_auto_reload() const {
    return disable_network_error_auto_reload_;
  }

#if BUILDFLAG(IS_ANDROID)
  void set_allow_intent_launches_in_background(bool value) {
    intent_launches_allowed_in_background_ = value;
  }
  bool are_intent_launches_allowed_in_background() const {
    return intent_launches_allowed_in_background_;
  }

  void SetResponse(
      std::unique_ptr<embedder_support::WebResourceResponse> response);
  std::unique_ptr<embedder_support::WebResourceResponse> TakeResponse();
#endif

 private:
  bool disable_network_error_auto_reload_ = false;

#if BUILDFLAG(IS_ANDROID)
  bool intent_launches_allowed_in_background_ = false;

  // Even though NavigationUIData is copyable, the WebResourceResponse would
  // only be used once since there are no network-retries applicable in this
  // case.
  class ResponseHolder : public base::RefCounted<ResponseHolder> {
   public:
    explicit ResponseHolder(
        std::unique_ptr<embedder_support::WebResourceResponse> response_);
    std::unique_ptr<embedder_support::WebResourceResponse> TakeResponse();

   private:
    friend class base::RefCounted<ResponseHolder>;
    virtual ~ResponseHolder();

    std::unique_ptr<embedder_support::WebResourceResponse> response_;
  };

  scoped_refptr<ResponseHolder> response_holder_;
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NAVIGATION_UI_DATA_IMPL_H_
