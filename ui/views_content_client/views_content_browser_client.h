// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_BROWSER_CLIENT_H_
#define UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_BROWSER_CLIENT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/content_browser_client.h"

namespace ui {

class ViewsContentClient;

class ViewsContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit ViewsContentBrowserClient(
      ViewsContentClient* views_content_client);

  ViewsContentBrowserClient(const ViewsContentBrowserClient&) = delete;
  ViewsContentBrowserClient& operator=(const ViewsContentBrowserClient&) =
      delete;

  ~ViewsContentBrowserClient() override;

  // content::ContentBrowserClient:
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;

 private:
  raw_ptr<ViewsContentClient> views_content_client_;
};

}  // namespace ui

#endif  // UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_BROWSER_CLIENT_H_
