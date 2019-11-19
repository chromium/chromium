// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/i18n_util.h"

#include "base/i18n/rtl.h"
#include "net/http/http_util.h"

namespace weblayer {
namespace i18n {

std::string GetApplicationLocale() {
  // The locale is set in ContentMainDelegateImpl::InitializeResourceBundle().
  return base::i18n::GetConfiguredLocale();
}

std::string GetAcceptLangs() {
  // TODO(estade): return more languages, not just the default.
  return net::HttpUtil::ExpandLanguageList(GetApplicationLocale());
}

}  // namespace i18n
}  // namespace weblayer
