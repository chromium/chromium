// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/content_renderer_client_impl.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "weblayer/renderer/ssl_error_helper.h"

#if defined(OS_ANDROID)
#include "android_webview/grit/aw_resources.h"
#include "android_webview/grit/aw_strings.h"
#endif

namespace weblayer {

namespace {

#if defined(OS_ANDROID)
constexpr char kThrottledErrorDescription[] =
    "Request throttled. Visit http://dev.chromium.org/throttling for more "
    "information.";

// Populates |error_html| (if it is not null), based on |error|.
// NOTE: This function is taken from
// AWContentRendererClient::PrepareErrorPage().
// TODO(1024326): If this implementation becomes the long-term
// implementation, this code should be shared rather than copied.
void PopulateErrorPageHTML(const blink::WebURLError& error,
                           std::string* error_html) {
  std::string err;
  if (error.reason() == net::ERR_TEMPORARILY_THROTTLED)
    err = kThrottledErrorDescription;
  else
    err = net::ErrorToString(error.reason());

  if (!error_html)
    return;

  // Create the error page based on the error reason.
  GURL gurl(error.url());
  std::string url_string = gurl.possibly_invalid_spec();
  int reason_id = IDS_AW_WEBPAGE_CAN_NOT_BE_LOADED;

  if (err.empty())
    reason_id = IDS_AW_WEBPAGE_TEMPORARILY_DOWN;

  std::string escaped_url = net::EscapeForHTML(url_string);
  std::vector<std::string> replacements;
  replacements.push_back(
      l10n_util::GetStringUTF8(IDS_AW_WEBPAGE_NOT_AVAILABLE));
  replacements.push_back(
      l10n_util::GetStringFUTF8(reason_id, base::UTF8ToUTF16(escaped_url)));

  // Having chosen the base reason, chose what extra information to add.
  if (reason_id == IDS_AW_WEBPAGE_TEMPORARILY_DOWN) {
    replacements.push_back(
        l10n_util::GetStringUTF8(IDS_AW_WEBPAGE_TEMPORARILY_DOWN_SUGGESTIONS));
  } else {
    replacements.push_back(err);
  }
  if (base::i18n::IsRTL())
    replacements.push_back("direction: rtl;");
  else
    replacements.push_back("");
  *error_html = base::ReplaceStringPlaceholders(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_AW_LOAD_ERROR_HTML),
      replacements, nullptr);
}
#endif  // OS_ANDROID

}  // namespace

ContentRendererClientImpl::ContentRendererClientImpl() = default;
ContentRendererClientImpl::~ContentRendererClientImpl() = default;

void ContentRendererClientImpl::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  SSLErrorHelper::Create(render_frame);
}

bool ContentRendererClientImpl::HasErrorPage(int http_status_code) {
  return http_status_code >= 400;
}

void ContentRendererClientImpl::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    std::string* error_html) {
  auto* ssl_helper = SSLErrorHelper::GetForFrame(render_frame);
  if (ssl_helper)
    ssl_helper->PrepareErrorPage();

#if defined(OS_ANDROID)
  PopulateErrorPageHTML(error, error_html);
#endif
}

}  // namespace weblayer
