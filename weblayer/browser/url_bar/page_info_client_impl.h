// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_URL_BAR_PAGE_INFO_CLIENT_IMPL_H_
#define WEBLAYER_BROWSER_URL_BAR_PAGE_INFO_CLIENT_IMPL_H_

#include "components/page_info/android/page_info_client.h"
#include "components/page_info/page_info_delegate.h"
#include "components/page_info/page_info_ui_delegate.h"

#include <memory>

namespace content {
class WebContents;
}

namespace weblayer {

// WebLayer's implementation of PageInfoClient.
class PageInfoClientImpl : public page_info::PageInfoClient {
 public:
  static PageInfoClientImpl* GetInstance();

  PageInfoClientImpl() = default;
  ~PageInfoClientImpl() = default;

  // PageInfoClient implementation.
  std::unique_ptr<PageInfoDelegate> CreatePageInfoDelegate(
      content::WebContents* web_contents) override;
  int GetJavaResourceId(int native_resource_id) override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_URL_BAR_PAGE_INFO_CLIENT_IMPL_H_
