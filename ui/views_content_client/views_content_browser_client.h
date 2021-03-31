// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_BROWSER_CLIENT_H_
#define UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_BROWSER_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/content_browser_client.h"

namespace ui {

class ViewsContentClient;

class ViewsContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit ViewsContentBrowserClient(
      ViewsContentClient* views_content_client);
  ~ViewsContentBrowserClient() override;

  // content::ContentBrowserClient:
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;

 private:
  ViewsContentClient* views_content_client_;

  DISALLOW_COPY_AND_ASSIGN(ViewsContentBrowserClient);
};

}  // namespace ui

#endif  // UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_BROWSER_CLIENT_H_
