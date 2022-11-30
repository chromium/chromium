// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_ui_data_impl.h"

#include "build/build_config.h"

namespace weblayer {

#if BUILDFLAG(IS_ANDROID)
NavigationUIDataImpl::ResponseHolder::ResponseHolder(
    std::unique_ptr<embedder_support::WebResourceResponse> response)
    : response_(std::move(response)) {}

NavigationUIDataImpl::ResponseHolder::~ResponseHolder() = default;

std::unique_ptr<embedder_support::WebResourceResponse>
NavigationUIDataImpl::ResponseHolder::TakeResponse() {
  return std::move(response_);
}

#endif  // BUILDFLAG(IS_ANDROID)

NavigationUIDataImpl::NavigationUIDataImpl() = default;
NavigationUIDataImpl::~NavigationUIDataImpl() = default;

std::unique_ptr<content::NavigationUIData> NavigationUIDataImpl::Clone() {
  auto rv = std::make_unique<NavigationUIDataImpl>();
  rv->disable_network_error_auto_reload_ = disable_network_error_auto_reload_;
#if BUILDFLAG(IS_ANDROID)
  rv->intent_launches_allowed_in_background_ =
      intent_launches_allowed_in_background_;
  rv->response_holder_ = response_holder_;
#endif  // BUILDFLAG(IS_ANDROID)
  return rv;
}

#if BUILDFLAG(IS_ANDROID)
void NavigationUIDataImpl::SetResponse(
    std::unique_ptr<embedder_support::WebResourceResponse> response) {
  DCHECK(!response_holder_);
  response_holder_ = base::MakeRefCounted<ResponseHolder>(std::move(response));
}

std::unique_ptr<embedder_support::WebResourceResponse>
NavigationUIDataImpl::TakeResponse() {
  if (!response_holder_)
    return nullptr;

  return response_holder_->TakeResponse();
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace weblayer
